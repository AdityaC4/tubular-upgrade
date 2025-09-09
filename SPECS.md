# Tubular Language Specifications

## Language Features

### Data Types

1. **int** - 32-bit signed integers
2. **double** - 64-bit floating-point numbers
3. **char** - Single characters (represented as integers)
4. **string** - Null-terminated character sequences

### Variable Declarations

```
TYPE IDENTIFIER;
TYPE IDENTIFIER = EXPRESSION;
```

Example:

```tube
int x;
double y = 3.14;
char c = 'A';
string s = "Hello";
```

### Functions

```
function IDENTIFIER(PARAMETERS) : RETURN_TYPE {
    STATEMENTS
}
```

Example:

```tube
function Add(int a, int b) : int {
    return a + b;
}
```

### Control Structures

#### If Statements

```tube
if (CONDITION) {
    STATEMENTS
} else {
    STATEMENTS
}
```

#### While Loops

```tube
while (CONDITION) {
    STATEMENTS
}
```

#### Break and Continue

- `break` - Exit the current loop
- `continue` - Skip to the next iteration of the current loop

### Expressions

#### Arithmetic Operations

- Addition: `+`
- Subtraction: `-`
- Multiplication: `*`
- Division: `/`
- Modulo: `%`
- Unary minus: `-`
- Square root: `sqrt(EXPRESSION)`

#### Comparison Operations

- Equal: `==`
- Not equal: `!=`
- Less than: `<`
- Less than or equal: `<=`
- Greater than: `>`
- Greater than or equal: `>=`

#### Logical Operations

- Logical AND: `&&`
- Logical OR: `||`
- Logical NOT: `!`

#### Type Casting

- `EXPRESSION : TYPE` - Cast expression to specified type
- Implicit conversions between numeric types (int to double)
- Explicit conversion functions: `ToInt`, `ToDouble`

### String Operations

- Concatenation: `+`
- Repetition: `*`
- Length: `size(STRING)`
- Indexing: `STRING[INDEX]`
- Assignment by index: `STRING[INDEX] = CHAR`

### Built-in Functions

Several helper functions are provided for string manipulation:

- `_alloc_str` - Allocate memory for a string
- `_strlen` - Calculate string length
- `_memcpy` - Copy memory
- `_strcat` - Concatenate strings
- `_int2string` - Convert integer to string
- `_str_cmp` - Compare strings

## WebAssembly Backend

### Memory Model

- Linear memory with a fixed initial size
- Strings are stored as null-terminated sequences
- Memory is managed through a global `$free_mem` pointer

### Type Mapping

- `int` → `i32`
- `double` → `f64`
- `char` → `i32`
- `string` → `i32` (memory address)

### Function Exports

All user-defined functions are exported with their original names, making them callable from the WebAssembly host environment.

## Compiler Architecture

### Frontend

1. **Lexer** - Tokenizes input source code using a DFA-based approach
2. **Parser** - Constructs an Abstract Syntax Tree (AST) using recursive descent parsing
3. **Symbol Table** - Manages variable and function declarations with scoping

### Backend

1. **Code Generation** - Translates AST to WAT (WebAssembly Text Format) using a visitor pattern
2. **Helper Functions** - Provides runtime support for string operations

### AST Node Types

The compiler uses a hierarchy of AST nodes to represent the program structure:

- Base `ASTNode` class with specialized subclasses for different language constructs
- Expression nodes for mathematical and logical operations
- Statement nodes for control flow
- Declaration nodes for variables and functions

### Visitor Pattern

The compiler uses the Visitor design pattern for code generation and other tree traversal operations:

- `ASTVisitor` base class defines visit methods for each node type
- `WATGenerator` is a concrete visitor that generates WAT code
- New visitors can be easily added by inheriting from `ASTVisitor`

## Example Programs

### Simple Function

```tube
function Add(int a, int b) : int {
    return a + b;
}
```

### String Manipulation

```tube
function Bracketize(string s) : string {
    return "[" + s + "]";
}
```

### Recursive Function

```tube
function Factorial(int n) : int {
    if (n <= 1) return 1;
    return n * Factorial(n - 1);
}
```

### Loop with Break/Continue

```tube
function CountDivSeven(double min, double max) : int {
    int value = min:int;
    int stop = max:int;
    int count = 0;

    if (value:double < min) value = value + 1;

    while (value <= stop) {
        if (value % 7 != 0) {
            value = value + 1;
            continue;
        }
        count = count + 1;
        value = value + 7;
    }
    return count;
}
```
