#include <assert.h>
#include <algorithm>
#include <chrono>
#include <complex>
#include <cstddef>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ASTNode.hpp"
#include "Control.hpp"
#include "FunctionInliningPass.hpp"
#include "LoopUnrollingPass.hpp"
#include "PassManager.hpp"
#include "SymbolTable.hpp"
#include "TailRecursionPass.hpp"
#include "TokenQueue.hpp"
#include "WATGenerator.hpp"
#include "lexer.hpp"

enum class PassId { Inline, Unroll, Tail };

static std::string TrimCopy(const std::string &input) {
  size_t start = 0;
  while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start]))) {
    ++start;
  }
  size_t end = input.size();
  while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
    --end;
  }
  return input.substr(start, end - start);
}

static std::vector<PassId> ParsePassOrderSpec(const std::string &spec) {
  std::vector<PassId> order;
  std::stringstream ss(spec);
  std::string token;
  while (std::getline(ss, token, ',')) {
    auto trimmed = TrimCopy(token);
    if (trimmed.empty()) {
      continue;
    }
    std::string lowered;
    lowered.reserve(trimmed.size());
    for (char ch : trimmed) {
      lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    PassId passId;
    if (lowered == "inline") {
      passId = PassId::Inline;
    } else if (lowered == "unroll") {
      passId = PassId::Unroll;
    } else if (lowered == "tail") {
      passId = PassId::Tail;
    } else {
      std::cout << "Error: Unknown pass '" << trimmed << "' in --pass-order (expected inline, unroll, tail)."
                << std::endl;
      exit(1);
    }

    if (std::find(order.begin(), order.end(), passId) != order.end()) {
      std::cout << "Error: Duplicate pass '" << trimmed << "' in --pass-order." << std::endl;
      exit(1);
    }
    order.push_back(passId);
  }

  if (order.size() != 3) {
    std::cout << "Error: --pass-order must specify inline, unroll, and tail exactly once." << std::endl;
    exit(1);
  }

  return order;
}

class Tubular {
private:
  using ast_ptr_t = std::unique_ptr<ASTNode>;
  using fun_ptr_t = std::unique_ptr<ASTNode_Function>;

  TokenQueue tokens;
  std::vector<fun_ptr_t> functions{};

  struct OpInfo {
    size_t level;
    char assoc; // l=left; r=right; n=non
  };
  std::unordered_map<std::string, OpInfo> op_map{};

  Control control;

  template <typename... Ts> void TriggerError(Ts... message) {
    if (tokens.None())
      tokens.Rewind();
    Error(tokens.CurFilePos(), std::forward<Ts>(message)...);
  }

  template <typename NODE_T, typename... ARG_Ts> static std::unique_ptr<NODE_T> MakeNode(ARG_Ts &&...args) {
    return std::make_unique<NODE_T>(std::forward<ARG_Ts>(args)...);
  }

  ast_ptr_t MakeVarNode(emplex::Token token) { return MakeNode<ASTNode_Var>(token, control.symbols); }

  Type GetReturnType(const ast_ptr_t &node_ptr) const { return node_ptr->ReturnType(control.symbols); }

  // Take in the provided node and add an ASTNode converter to make it a double,
  // as needed. Return if a change was made.
  ast_ptr_t PromoteToDouble(ast_ptr_t &&node_ptr) {
    if (!node_ptr->ReturnType(control.symbols).IsDouble()) {
      return MakeNode<ASTNode_ToDouble>(std::move(node_ptr));
    }
    return node_ptr;
  }

  // Take in the provided node and add an ASTNode converter to make it a double,
  // as needed. Return if a change was made.
  ast_ptr_t DemoteToInt(ast_ptr_t &&node_ptr) {
    if (node_ptr->ReturnType(control.symbols).IsDouble()) {
      return MakeNode<ASTNode_ToInt>(std::move(node_ptr));
    }
    return node_ptr;
  }

  void SetupOperators() {
    // Setup operator precedence.
    size_t cur_prec = 0;
    op_map["("] = op_map["!"] = OpInfo{cur_prec++, 'n'};
    op_map["*"] = op_map["/"] = op_map["%"] = OpInfo{cur_prec++, 'l'};
    op_map["+"] = op_map["-"] = OpInfo{cur_prec++, 'l'};
    op_map["<"] = op_map["<="] = op_map[">"] = op_map[">="] = OpInfo{cur_prec++, 'n'};
    op_map["=="] = op_map["!="] = OpInfo{cur_prec++, 'n'};
    op_map["&&"] = OpInfo{cur_prec++, 'l'};
    op_map["||"] = OpInfo{cur_prec++, 'l'};
    op_map["="] = OpInfo{cur_prec++, 'r'};
  }

public:
  Tubular(std::string filename) {
    std::ifstream in_file(filename); // Load the input file
    if (in_file.fail()) {
      std::cerr << "ERROR: Unable to open file '" << filename << "'." << std::endl;
      exit(1);
    }

    tokens.Load(in_file); // Load all tokens from the file.

    SetupOperators();
  }

  // Convert any token representing a unary value into an ASTNode.
  // (i.e., a leaf in an expression and associated unary operators)
  ast_ptr_t Parse_UnaryTerm() {
    const emplex::Token &token = tokens.Use();

    if (token == '+')
      return Parse_UnaryTerm(); // (Operator + does nothing...)

    if (token == '-' || token == '!') { // Add node for unary prefix
      return MakeNode<ASTNode_Math1>(token, Parse_UnaryTerm());
    }

    // Check main terms.
    ast_ptr_t out;
    switch (token.id) {
    case '(': // Allow full expressions in parentheses.
      out = Parse_Expression();
      tokens.Use(')');
      break;
    case emplex::Lexer::ID_ID:
      if (!control.symbols.Has(token.lexeme)) {
        Error(token, "Unknown variable '", token.lexeme, "'.");
      }
      out = MakeVarNode(token);

      if (tokens.Is('(')) {
        out = Parse_FunctionCall(token);
      } else if (tokens.Is('[')) {
        out = Parse_Index(token, std::move(out));
      }
      break;
    case emplex::Lexer::ID_LIT_INT:
      out = MakeNode<ASTNode_IntLit>(token, std::stoi(token.lexeme));
      break;
    case emplex::Lexer::ID_LIT_CHAR:
      out = MakeNode<ASTNode_CharLit>(token, token.lexeme[1]);
      break;
    case emplex::Lexer::ID_LIT_FLOAT:
      out = MakeNode<ASTNode_FloatLit>(token, std::stod(token.lexeme));
      break;
    case emplex::Lexer::ID_LIT_STRING: {
      out = MakeNode<ASTNode_StringLit>(token, token.lexeme.substr(1, token.lexeme.size() - 2));
      break;
    }
    case emplex::Lexer::ID_SQRT:
      tokens.Use('(');
      out = PromoteToDouble(Parse_Expression());
      tokens.Use(')');
      out = MakeNode<ASTNode_Math1>(token, std::move(out));
      break;
    case emplex::Lexer::ID_SIZE: {
      tokens.Use('(');
      out = Parse_Expression();
      tokens.Use(')');
      out = MakeNode<ASTNode_Size>(token, std::move(out));
      break;
    }
    default:
      Error(token, "Unexpected token '", token.lexeme, "'");
    }

    // @CAO Check for '(' or '[' to know if this is a function call or array
    // index?

    // Check to see if the term is followed by a type modifier.
    if (tokens.UseIf(':')) {
      auto type_token = tokens.Use(emplex::Lexer::ID_TYPE, "Expected a type specified after ':'.");
      if (type_token.lexeme == "double")
        out = MakeNode<ASTNode_ToDouble>(std::move(out));
      else if (type_token.lexeme == "int")
        out = MakeNode<ASTNode_ToInt>(std::move(out));
      else if (type_token.lexeme == "string")
        out = MakeNode<ASTNode_ToString>(std::move(out));
    }

    return out;
  }

  // Parse function calls
  ast_ptr_t Parse_FunctionCall(const emplex::Token &token) {
    tokens.Use('(');
    std::vector<ast_ptr_t> args;

    while (!tokens.UseIf(')')) {
      args.push_back(Parse_Expression());
      if (!tokens.UseIf(',') && !tokens.Is(')')) {
        Error(token, "Expected ',' or ')' in function call arguments.");
      }
    }

    size_t fun_id = control.symbols.GetVarID(token.lexeme);
    const auto &fun_type = control.symbols.At(fun_id).type;

    if (args.size() != fun_type.NumParams()) {
      Error(token, "Function '", token.lexeme, "' expects ", fun_type.NumParams(), " arguments but got ", args.size(),
            ".");
    }
    for (size_t i = 0; i < args.size(); ++i) {
      if (!args[i]->ReturnType(control.symbols).ConvertToOK(fun_type.ParamType(i))) {
        Error(args[i]->GetFilePos(), "Argument ", i + 1, " of function '", token.lexeme, "' has type mismatch.");
      }
    }

    return MakeNode<ASTNode_FunctionCall>(token, fun_id, std::move(args));
  }

  ast_ptr_t Parse_Index(const emplex::Token &token, ast_ptr_t identifier_node) {
    tokens.Use('[');
    auto index = Parse_Expression();
    tokens.Use(']');
    return MakeNode<ASTNode_Indexing>(token, std::move(identifier_node), std::move(index));
  }

  // Parse expressions.  The level input determines how restrictive this parse
  // should be. Only continue processing with types at the target level or
  // higher.
  ast_ptr_t Parse_Expression(size_t prec_limit = 1000) {
    // Any expression must begin with a variable name or a literal value.
    ast_ptr_t cur_node = Parse_UnaryTerm();

    size_t skip_prec = 1000; // If we get a non-associative op, we must skip next one.

    // While there are more tokens to process, try to expand this expression.
    while (tokens.Any()) {
      // Peek at the next token; if it is an op, keep going and get its info.
      auto op_token = tokens.Peek();
      if (!op_map.count(op_token.lexeme))
        break; // Not an op token; stop here!
      OpInfo op_info = op_map[op_token.lexeme];

      // If precedence of next operator is too high, return what we have.
      if (op_info.level > prec_limit)
        break;

      // If the next precedence is not allowed, throw an error.
      if (op_info.level == skip_prec) {
        Error(op_token, "Operator '", op_token.lexeme, "' is non-associative.");
      }

      // If we made it here, we have a binary operation to use, so consume it.
      tokens.Use();

      // Find the allowed precedence for the next term.
      size_t next_limit = op_info.level;
      if (op_info.assoc != 'r')
        --next_limit;

      // Load the next term.
      ast_ptr_t node2 = Parse_Expression(next_limit);

      // Build the new node.
      cur_node = MakeNode<ASTNode_Math2>(op_token, std::move(cur_node), std::move(node2));

      // If operator is non-associative, skip the current precedence for next
      // loop.
      skip_prec = (op_info.assoc == 'n') ? op_info.level : 1000;
    }

    return cur_node;
  }

  ast_ptr_t Parse_Statement() {
    // Test what kind of statement this is and call the appropriate function...
    switch (tokens.Peek()) {
      using namespace emplex;
    case Lexer::ID_TYPE:
      return Parse_Statement_Declare();
    case Lexer::ID_IF:
      return Parse_Statement_If();
    case Lexer::ID_WHILE:
      return Parse_Statement_While();
    case Lexer::ID_RETURN:
      return Parse_Statement_Return();
    case Lexer::ID_BREAK:
      return Parse_Statement_Break();
    case Lexer::ID_CONTINUE:
      return Parse_Statement_Continue();
    case '{':
      return Parse_StatementList();
    case ';':
      tokens.Use();
      return nullptr;
    default:
      return Parse_Statement_Expression();
    }
  }

  ast_ptr_t Parse_Statement_Declare() {
    auto type_token = tokens.Use();
    const auto id_token = tokens.Use(emplex::Lexer::ID_ID, "Declarations must have a type followed by identifier.");
    control.symbols.AddVar(type_token, id_token);
    if (tokens.UseIf(';')) {
      return nullptr; // Variable added, nothing else to do.
    }
    auto op_token = tokens.Use('=', "Expected ';' or '=' after declaration of variable '", id_token.lexeme, "'.");
    auto rhs_node = Parse_Expression();
    tokens.Use(';');

    auto lhs_node = MakeVarNode(id_token);
    Type lhs_type = lhs_node->ReturnType(control.symbols);
    Type rhs_type = rhs_node->ReturnType(control.symbols);

    return MakeNode<ASTNode_Math2>(id_token, "=", std::move(lhs_node), std::move(rhs_node));
  }

  ast_ptr_t Parse_Statement_If() {
    auto if_token = tokens.Use(emplex::Lexer::ID_IF);
    tokens.Use('(', "If commands must be followed by a '(");
    ast_ptr_t condition = Parse_Expression();
    tokens.Use(')');
    ast_ptr_t action = Parse_Statement();

    // Check if we need to add on an "else" branch
    if (tokens.UseIf(emplex::Lexer::ID_ELSE)) {
      ast_ptr_t alt = Parse_Statement();
      return MakeNode<ASTNode_If>(if_token, std::move(condition), std::move(action), std::move(alt));
    }

    return MakeNode<ASTNode_If>(if_token, std::move(condition), std::move(action));
  }

  ast_ptr_t Parse_Statement_While() {
    auto while_token = tokens.Use(emplex::Lexer::ID_WHILE);
    tokens.Use('(', "While commands must be followed by a '(");
    ast_ptr_t condition = Parse_Expression();
    tokens.Use(')');
    ast_ptr_t action = Parse_Statement();
    return MakeNode<ASTNode_While>(while_token, std::move(condition), std::move(action));
  }

  ast_ptr_t Parse_Statement_Return() {
    auto token = tokens.Use(emplex::Lexer::ID_RETURN);
    ast_ptr_t return_expr = Parse_Statement_Expression();
    return MakeNode<ASTNode_Return>(token, std::move(return_expr));
  }

  ast_ptr_t Parse_Statement_Break() {
    auto token = tokens.Use(emplex::Lexer::ID_BREAK);
    return MakeNode<ASTNode_Break>(token);
  }

  ast_ptr_t Parse_Statement_Continue() {
    auto token = tokens.Use(emplex::Lexer::ID_CONTINUE);
    return MakeNode<ASTNode_Continue>(token);
  }

  ast_ptr_t Parse_Statement_Expression() {
    ast_ptr_t out = Parse_Expression();
    tokens.Use(';');
    return out;
  }

  ast_ptr_t Parse_StatementList() {
    auto out_node = MakeNode<ASTNode_Block>(tokens.Peek());
    tokens.Use('{', "Statement blocks must start with '{'.");
    control.symbols.PushScope();
    while (tokens.Any() && tokens.Peek() != '}') {
      ast_ptr_t statement = Parse_Statement();
      if (statement)
        out_node->AddChild(std::move(statement));
    }
    control.symbols.PopScope();
    tokens.Use('}', "Statement blocks must end with '}'.");
    return out_node;
  }

  // A function has the format:
  //    function ID ( PARAMETERS ) : TYPE { STATEMENT_BLOCK }
  //    The initial ID is the function name.
  //    PARAMETERS can be empty or a series of comma-separated "TYPE ID"
  //    declaring parameters TYPE can be int, char, or double and is used as the
  //    return type. STATEMENT BLOCK is a series of statements to run, ending in
  //    a return statement.
  fun_ptr_t Parse_Function() {
    using namespace emplex;
    tokens.Use(Lexer::ID_FUNCTION, "Outermost scope must define functions.");
    control.symbols.PushScope(); // Enter a special scope for the function.
    auto name_token = tokens.Use(Lexer::ID_ID, "Function must have a name.");
    tokens.Use('(', "Function declaration must have '(' after name.");
    std::vector<size_t> param_ids;
    std::vector<Type> param_types;
    while (!tokens.UseIf(')')) {
      auto type_token = tokens.Use(Lexer::ID_TYPE);
      param_types.emplace_back(type_token);
      const auto id_token =
          tokens.Use(emplex::Lexer::ID_ID, "Function parameters must have a type followed by identifier.");
      size_t param_id = control.symbols.AddVar(type_token, id_token);
      param_ids.push_back(param_id);
      if (!tokens.UseIf(',') && !tokens.Is(')')) {
        TriggerError("Parameters must be separated by commas (','; found '", tokens.Peek().lexeme, "'.");
      }
    }
    tokens.Use(':');
    Type return_type(tokens.Use(Lexer::ID_TYPE));

    // Now that we have the function signature, let the symbol table know about
    // it.
    size_t fun_id = control.symbols.AddFunction(name_token, param_types, return_type);

    // Now parse the body of this function.
    control.symbols.ClearFunctionVars();
    ast_ptr_t body = Parse_StatementList();
    control.symbols.PopScope(); // Leave the function scope.

    if (!body->IsReturn()) {
      Error(name_token, "Function '", name_token.lexeme, "' must guarantee a return statement through all paths.");
    }

    auto out_node = MakeNode<ASTNode_Function>(name_token, fun_id, param_ids, std::move(body));
    out_node->SetVars(control.symbols.GetFunctionVars());
    return out_node;
  }

  void Parse() {
    // Outer layer can only be function definitions.
    while (tokens.Any()) {
      functions.push_back(Parse_Function());
      functions.back()->TypeCheck(control.symbols);
    }
  }

  void ToWAT() {
    control.Code("(module");
    control.Indent(2);

    // Manage DATA (USED IN PROJECT 4!!)
    control.CommentLine(";; Define a memory block with ten pages (640KB)");
    control.Code("(memory (export \"memory\") 1)")
        .Code("(data (i32.const 0) \"0\\00\")")
        .Code("(data (i32.const 2) \"0123456789\\00\")")
        .Code("(data (i32.const 13) \"\\00\")");
    for (auto &fun_ptr : functions) {
      fun_ptr->InitializeWAT(control);
    }
    control.Code("(global $free_mem (mut i32) (i32.const ", control.wat_mem_pos, "))").Code("");

    control
        .Code(";; Function to allocate a string; add one to size and places "
              "null there.")
        .Code("(func $_alloc_str (param $size i32) (result i32)")
        .Code("  (local $null_pos i32) ;; Local variable to place null "
              "terminator.")
        .Code("  (global.get $free_mem)")
        .Comment("Old free mem is alloc start.")
        .Code("  (global.get $free_mem)")
        .Comment("Adjust new free mem.")
        .Code("  (local.get $size)")
        .Code("  (i32.add)")
        .Code("  (local.set $null_pos)")
        .Code("  (i32.store8 (local.get $null_pos) (i32.const 0))")
        .Comment("Place null terminator.")
        .Code("  (i32.add (i32.const 1) (local.get $null_pos))")
        .Code("  (global.set $free_mem)")
        .Comment("Update free memory start.")
        .Code(")")
        .Code("");

    // LOTS OF OTHER HELPER FUNCTIONS SHOULD GO HERE FOR PROJECT 4!!
    // strlen
    control.Code(";; Function to calculate the length of a null-terminated string.")
        .Code("(func $_strlen (param $str i32) (result i32)")
        .Code("  (local $length i32) ;; Local variable to store the string "
              "length.")
        .Code("  (local.set $length (i32.const 0)) ;; Initialize length to 0.")
        .Code("  (block $exit ;; Outer block for loop termination.")
        .Code("    (loop $check")
        .Code("      (br_if $exit (i32.eq (i32.load8_u (local.get $str)) "
              "(i32.const 0)))")
        .Comment("If the current byte is null, exit the loop.")
        .Code("      (local.set $str (i32.add (local.get $str) (i32.const 1)))")
        .Comment("Increment the pointer and the length counter.")
        .Code("      (local.set $length (i32.add (local.get $length) "
              "(i32.const 1)))")
        .Code("      (br $check)")
        .Comment("Continue the loop.")
        .Code("    )")
        .Code("  )")
        .Code("  (local.get $length) ;; Return the calculated length.")
        .Code(")")
        .Code("");

    // memcpy
    control
        .Code(";; Function to copy a specific number of bytes from one "
              "location to another.")
        .Code("(func $_memcpy (param $src i32) (param $dest i32) (param $size "
              "i32)")
        .Code("  (block $done")
        .Code("    (loop $copy")
        .Code("      (br_if $done (i32.eqz (local.get $size)))")
        .Comment("Exit the loop when $size reaches 0.")
        .Code("      (i32.store8 (local.get $dest) (i32.load8_u (local.get "
              "$src)))")
        .Comment("Copy the current byte from source to destination.")
        .Code("      (local.set $src (i32.add (local.get $src) (i32.const 1)))")
        .Comment("Increment source and destination pointers.")
        .Code("      (local.set $dest (i32.add (local.get $dest) (i32.const 1)))")
        .Comment("Decrement size.")
        .Code("      (local.set $size (i32.sub (local.get $size) (i32.const 1)))")
        .Code("      (br $copy)")
        .Comment("Repeat the loop.")
        .Code("    )")
        .Code("  )")
        .Code(")")
        .Code("");

    // strcat
    control.Code(";; Function to concatenate two strings.")
        .Code("(func $_strcat (param $str1 i32) (param $str2 i32) (result i32)")
        .Code("  (local $len1 i32) ;; Length of the first string.")
        .Code("  (local $len2 i32) ;; Length of the second string.")
        .Code("  (local $result i32) ;; Pointer to the new concatenated string.")
        .Code("  ;; Calculate the length of the first string.")
        .Code("  (local.set $len1 (call $_strlen (local.get $str1)))")
        .Code("  ;; Calculate the length of the second string.")
        .Code("  (local.set $len2 (call $_strlen (local.get $str2)))")
        .Code("  ;; Allocate memory for the concatenated string using "
              "_alloc_str.")
        .Code("  (local.set $result (call $_alloc_str (i32.add (local.get "
              "$len1) (local.get $len2))))")
        .Code("  ;; Copy the first string into the allocated memory.")
        .Code("  (call $_memcpy (local.get $str1) (local.get $result) "
              "(local.get $len1))")
        .Code("  ;; Copy the second string immediately after the first string "
              "in the allocated memory.")
        .Code("  (call $_memcpy (local.get $str2) (i32.add (local.get $result) "
              "(local.get $len1)) (local.get $len2)) "
              ";; Include null terminator.")
        .Code("  ;; Return the pointer to the concatenated string.")
        .Code("  (local.get $result)")
        .Code(")")
        .Code("");

    // swap
    control.Code(";; Function to swap the first two values on the stack.")
        .Code("(func $_swap (param $a i32) (param $b i32) (result i32 i32)")
        .Code("  (local.get $b)")
        .Code("  (local.get $a)")
        .Code(")")
        .Code("");

    // repeat string
    control.Code(";; Function to repeat a string a given number of times")
        .Code("(func $_repeat_string (param $str i32) (param $count i32) "
              "(result i32)")
        .Code("  (local $result i32)")
        .Comment("Pointer to the resulting string")
        .Code("  (local $str_len i32)")
        .Comment("Length of the input string")
        .Code("  (local $total_len i32)")
        .Comment("Total length of the resulting string")
        .Code("  (local $temp_dest i32)")
        .Comment("Temporary pointer for destination")
        .Code("  (local.set $str_len (call $_strlen (local.get $str)))")
        .Code("  (local.set $total_len (i32.mul (local.get $str_len) "
              "(local.get $count)))")
        .Code("  (local.set $result (call $_alloc_str (local.get $total_len)))")
        .Code("  (local.set $temp_dest (local.get $result))")
        .Code("  (block $exit_loop")
        .Code("    (loop $repeat_loop")
        .Code("      (br_if $exit_loop (i32.eqz (local.get $count)))")
        .Code("      (call $_memcpy (local.get $str) (local.get $temp_dest) "
              "(local.get $str_len))")
        .Code("      (local.set $temp_dest (i32.add (local.get $temp_dest) "
              "(local.get $str_len)))")
        .Code("      (local.set $count (i32.sub (local.get $count) (i32.const "
              "1)))")
        .Code("      (br $repeat_loop)")
        .Code("    )")
        .Code("  )")
        .Code("  (local.get $result)")
        .Code(")")
        .Code("");

    // int2string
    control.Code("(func $_int2string (param $var0 i32) (result i32)")
        .Code("  (local $var2 i32)")
        .Code("  (local $var3 i32)")
        .Code("  (local $var4 i32)")
        .Code("  (local $temp0 i32)")
        .Code("  (local $temp1 i32)")
        .Code("  (local.get $var0)")
        .Code("  (i32.const 0)")
        .Code("  (i32.eq)")
        .Code("  (if")
        .Code("    (then")
        .Code("      (i32.const 0)")
        .Code("      (return)")
        .Code("    )")
        .Code("  )")
        .Code("  (i32.const 2)")
        .Code("  (local.set $var2)")
        .Code("  (i32.const 0)")
        .Code("  (local.set $var3)")
        .Code("  (local.get $var0)")
        .Code("  (i32.const 0)")
        .Code("  (i32.lt_s)")
        .Code("  (if")
        .Code("    (then")
        .Code("      (i32.const 1)")
        .Code("      (local.set $var3)")
        .Code("      (local.get $var0)")
        .Code("      (i32.const 0)")
        .Code("      (i32.const 1)")
        .Code("      (i32.sub)")
        .Code("      (i32.mul)")
        .Code("      (local.set $var0)")
        .Code("    )")
        .Code("  )")
        .Code("  (i32.const 13)")
        .Code("  (local.set $var4)")
        .Code("  (block $exit1")
        .Code("    (loop $loop1")
        .Code("      (local.get $var0)")
        .Code("      (i32.const 0)")
        .Code("      (i32.gt_s)")
        .Code("      (i32.eqz)")
        .Code("      (br_if $exit1)")
        .Code("      (i32.const 2)")
        .Code("      call $_alloc_str")
        .Code("      (local.set $temp0)")
        .Code("      (local.get $temp0)")
        .Code("      (local.get $var2)")
        .Code("      (local.get $var0)")
        .Code("      (i32.const 10)")
        .Code("      (i32.rem_s)")
        .Code("      (i32.add)")
        .Code("      (i32.load8_u)")
        .Code("      i32.store8")
        .Code("      (local.get $temp0)")
        .Code("      (local.get $var4)")
        .Code("      call $_strcat")
        .Code("      (local.set $var4)")
        .Code("      (local.get $var0)")
        .Code("      (i32.const 10)")
        .Code("      (i32.div_s)")
        .Code("      (local.set $var0)")
        .Code("      (br $loop1)")
        .Code("    )")
        .Code("  )")
        .Code("  (local.get $var3)")
        .Code("  (if")
        .Code("    (then")
        .Code("      (i32.const 2)")
        .Code("      call $_alloc_str")
        .Code("      (local.set $temp1)")
        .Code("      (local.get $temp1)")
        .Code("      (i32.const 45)")
        .Code("      i32.store8")
        .Code("      (local.get $temp1)")
        .Code("      (local.get $var4)")
        .Code("      call $_strcat")
        .Code("      (local.set $var4)")
        .Code("    )")
        .Code("  )")
        .Code("  (local.get $var4)")
        .Code(")")
        .Code("");

    // string compare
    control.Code("(func $_str_cmp (param $lhs i32) (param $rhs i32) (result i32)")
        .Code("  (local $len1 i32)")
        .Code("  (local $len2 i32)")
        .Code("  (local.set $len1 (call $_strlen (local.get $lhs)))")
        .Code("  (local.set $len2 (call $_strlen (local.get $rhs)))")
        .Code("  (i32.ne (local.get $len1) (local.get $len2))")
        .Code("  (if (then")
        .Code("    (return (i32.const 0))")
        .Code("  ))")
        .Code("  (block $exit")
        .Code("    (loop $compare")
        .Code("      (i32.eqz (local.get $len1))")
        .Code("      (br_if $exit)")
        .Code("      (i32.load8_u (local.get $lhs))")
        .Code("      (i32.load8_u (local.get $rhs))")
        .Code("      (i32.ne)")
        .Code("      (if (then")
        .Code("        (return (i32.const 0))")
        .Code("      ))")
        .Code("      (local.set $lhs (i32.add (local.get $lhs) (i32.const 1)))")
        .Code("      (local.set $rhs (i32.add (local.get $rhs) (i32.const 1)))")
        .Code("      (local.set $len1 (i32.sub (local.get $len1) (i32.const 1)))")
        .Code("      (br $compare)")
        .Code("    )")
        .Code("  )")
        .Code("  (i32.const 1)")
        .Code(")")
        .Code("");

    // Generate code for each function using the visitor pattern
    for (auto &fun_ptr : functions) {
      // Create a WAT generator visitor and use it to generate code
      WATGenerator generator(control);
      fun_ptr->Accept(generator);
    }
    control.Indent(-2);
    control.Code(")").Comment("END program module");
  }

  void PrintCode() const { control.PrintCode(); }
  void PrintSymbols() const { control.symbols.Print(); }
  
  // Get the total size of generated code (for performance comparison)
  size_t GetCodeSize() const {
    size_t totalSize = 0;
    for (const auto &line : control.code) {
      totalSize += line.code.size() + line.comment.size() + line.indent;
    }
    return totalSize;
  }
  
  void PrintAST() const {
    for (auto &fun_ptr : functions) {
      fun_ptr->Print();
    }
  }

  // New method to run optimization passes
  void RunOptimizationPasses(bool enableLoopUnrolling = true, int unrollFactor = 4,
                             bool enableFunctionInlining = true, bool enableTailLoopify = true,
                             const std::vector<PassId> &passOrder = {}) {
    PassManager passManager;

    auto addInlinePass = [&]() {
      passManager.addPass(std::make_unique<FunctionInliningPass>(control.symbols, true, false, false, 3, 40, 100));
    };
    auto addUnrollPass = [&]() {
      passManager.addPass(std::make_unique<LoopUnrollingPass>(unrollFactor, false, false, 100, false));
    };
    auto addTailPass = [&]() {
      passManager.addPass(
          std::make_unique<TailRecursionPass>(control.symbols, enableTailLoopify, false, false, 1000));
    };

    std::vector<PassId> effectiveOrder = passOrder;
    if (effectiveOrder.empty()) {
      effectiveOrder = {PassId::Inline, PassId::Unroll, PassId::Tail};
    }

    for (PassId id : effectiveOrder) {
      switch (id) {
      case PassId::Inline:
        if (enableFunctionInlining) {
          addInlinePass();
        }
        break;
      case PassId::Unroll:
        if (enableLoopUnrolling) {
          addUnrollPass();
        }
        break;
      case PassId::Tail:
        addTailPass();
        break;
      }
    }

    // Run all passes on each function
    for (auto &fun_ptr : functions) {
      passManager.runPasses(*fun_ptr);
    }
  }
};

void printHelp(const char* programName) {
  std::cout << "Tubular Compiler - A compiler for the Tubular language\n\n";
  std::cout << "USAGE:\n";
  std::cout << "  " << programName << " <filename> [OPTIONS]\n\n";
  std::cout << "ARGUMENTS:\n";
  std::cout << "  filename    Input Tubular source file to compile\n\n";
  std::cout << "OPTIONS:\n";
  std::cout << "  --help, -h              Show this help message and exit\n";
  std::cout << "  --no-unroll             Disable loop unrolling optimization\n";
  std::cout << "  --unroll-factor=N       Set loop unrolling factor (1-16, default: 4)\n";
  std::cout << "                          Setting to 1 effectively disables unrolling\n";
  std::cout << "  --no-inline             Disable function inlining optimization\n";
  std::cout << "  --tail=loop|off         Control tail recursion optimization\n";
  std::cout << "                          loop: Convert tail recursion to loops (default)\n";
  std::cout << "                          off:  Disable tail recursion optimization\n\n";
  std::cout << "  --pass-order=a,b,c      Set optimization pass order using a permutation of\n";
  std::cout << "                          inline,unroll,tail (default: inline,unroll,tail)\n\n";
  std::cout << "EXAMPLES:\n";
  std::cout << "  " << programName << " program.tub              # Compile with default optimizations\n";
  std::cout << "  " << programName << " program.tub --no-unroll  # Disable loop unrolling\n";
  std::cout << "  " << programName << " program.tub --unroll-factor=8 --no-inline  # Custom settings\n";
  std::cout << "  " << programName << " program.tub --tail=off   # Disable tail recursion optimization\n\n";
  std::cout << "OPTIMIZATION PASSES:\n";
  std::cout << "  The compiler includes several optimization passes:\n";
  std::cout << "  • Function Inlining: Inlines small, pure functions to reduce call overhead\n";
  std::cout << "  • Loop Unrolling: Unrolls loops to reduce branch overhead and enable\n";
  std::cout << "    further optimizations\n";
  std::cout << "  • Tail Recursion: Converts tail-recursive functions to iterative loops\n\n";
  std::cout << "OUTPUT:\n";
  std::cout << "  The compiler generates WebAssembly Text (WAT) format output to stdout.\n";
  std::cout << "  Redirect to a file to save: " << programName << " program.tub > output.wat\n";
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cout << "Error: No input file specified\n\n";
    printHelp(argv[0]);
    exit(1);
  }

  // Check for help flag
  std::string firstArg = argv[1];
  if (firstArg == "--help" || firstArg == "-h") {
    printHelp(argv[0]);
    exit(0);
  }

  std::string filename = argv[1];
  bool enableLoopUnrolling = true;
  int unrollFactor = 4; // default
  bool enableFunctionInlining = true; // default
  bool enableTailLoopify = true;      // default
  std::vector<PassId> passOrder = {PassId::Inline, PassId::Unroll, PassId::Tail};

  // Track seen flags for validation
  bool seenNoUnroll = false;
  bool seenUnrollFactor = false;
  bool seenTail = false;

  // Check for optimization flags (allow multiple)
  for (int i = 2; i < argc; ++i) {
    std::string flag = argv[i];
    if (flag == "--no-unroll") {
      enableLoopUnrolling = false;
      seenNoUnroll = true;
    } else if (flag == "--no-inline") {
      enableFunctionInlining = false;
    } else if (flag.rfind("--unroll-factor=", 0) == 0) {
      std::string factorStr = flag.substr(16); // length of "--unroll-factor="
      try {
        if (seenUnrollFactor) {
          std::cout << "Error: Duplicate --unroll-factor specified" << std::endl;
          exit(1);
        }
        unrollFactor = std::stoi(factorStr);
        if (unrollFactor < 1 || unrollFactor > 16) {
          std::cout << "Error: Unroll factor must be between 1 and 16" << std::endl;
          exit(1);
        }
        // If unroll factor is 1, disable loop unrolling entirely
        if (unrollFactor == 1) {
          enableLoopUnrolling = false;
        }
        seenUnrollFactor = true;
      } catch (const std::exception&) {
        std::cout << "Error: Invalid unroll factor '" << factorStr << "'" << std::endl;
        exit(1);
      }
    } else if (flag.rfind("--tail=", 0) == 0) {
      std::string mode = flag.substr(7);
      if (mode == "loop") {
        if (seenTail && !enableTailLoopify) {
          std::cout << "Error: Conflicting --tail options: both 'off' and 'loop' specified" << std::endl;
          exit(1);
        }
        enableTailLoopify = true;
      } else if (mode == "off") {
        if (seenTail && enableTailLoopify) {
          std::cout << "Error: Conflicting --tail options: both 'loop' and 'off' specified" << std::endl;
          exit(1);
        }
        enableTailLoopify = false;
      } else {
        std::cout << "Error: Unknown tail mode '" << mode << "' (use loop|off)" << std::endl;
        exit(1);
      }
      seenTail = true;
    } else if (flag.rfind("--pass-order=", 0) == 0) {
      std::string spec = flag.substr(13);
      if (spec.empty()) {
        std::cout << "Error: --pass-order requires a comma-separated permutation of inline,unroll,tail" << std::endl;
        exit(1);
      }
      passOrder = ParsePassOrderSpec(spec);
    } else {
      std::cout << "Error: Unknown flag '" << flag << "'" << std::endl;
      exit(1);
    }
  }

  // Validate combinations after parsing
  if (seenNoUnroll && seenUnrollFactor && unrollFactor > 1) {
    std::cout << "Error: Cannot combine --no-unroll with --unroll-factor=" << unrollFactor
              << ". Use one or set --unroll-factor=1 to disable unrolling." << std::endl;
    exit(1);
  }

  Tubular prog(filename);
  prog.Parse();

  // Run optimization passes
  prog.RunOptimizationPasses(enableLoopUnrolling, unrollFactor, enableFunctionInlining, enableTailLoopify, passOrder);

  // -- uncomment for debugging --
  // prog.PrintSymbols();
  // prog.PrintAST();

  prog.ToWAT();
  prog.PrintCode();
}
