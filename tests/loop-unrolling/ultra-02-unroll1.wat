(module
  ;; ;; Define a memory block with ten pages (640KB)
  (memory (export "memory") 1)
  (data (i32.const 0) "0\00")
  (data (i32.const 2) "0123456789\00")
  (data (i32.const 13) "\00")
  (global $free_mem (mut i32) (i32.const 14))
  
  ;; Function to allocate a string; add one to size and places null there.
  (func $_alloc_str (param $size i32) (result i32)
    (local $null_pos i32) ;; Local variable to place null terminator.
    (global.get $free_mem)                                                   ;; Old free mem is alloc start.
    (global.get $free_mem)                                                   ;; Adjust new free mem.
    (local.get $size)
    (i32.add)
    (local.set $null_pos)
    (i32.store8 (local.get $null_pos) (i32.const 0))                         ;; Place null terminator.
    (i32.add (i32.const 1) (local.get $null_pos))
    (global.set $free_mem)                                                   ;; Update free memory start.
  )
  
  ;; Function to calculate the length of a null-terminated string.
  (func $_strlen (param $str i32) (result i32)
    (local $length i32) ;; Local variable to store the string length.
    (local.set $length (i32.const 0)) ;; Initialize length to 0.
    (block $exit ;; Outer block for loop termination.
      (loop $check
        (br_if $exit (i32.eq (i32.load8_u (local.get $str)) (i32.const 0)))  ;; If the current byte is null, exit the loop.
        (local.set $str (i32.add (local.get $str) (i32.const 1)))            ;; Increment the pointer and the length counter.
        (local.set $length (i32.add (local.get $length) (i32.const 1)))
        (br $check)                                                          ;; Continue the loop.
      )
    )
    (local.get $length) ;; Return the calculated length.
  )
  
  ;; Function to copy a specific number of bytes from one location to another.
  (func $_memcpy (param $src i32) (param $dest i32) (param $size i32)
    (block $done
      (loop $copy
        (br_if $done (i32.eqz (local.get $size)))                            ;; Exit the loop when $size reaches 0.
        (i32.store8 (local.get $dest) (i32.load8_u (local.get $src)))        ;; Copy the current byte from source to destination.
        (local.set $src (i32.add (local.get $src) (i32.const 1)))            ;; Increment source and destination pointers.
        (local.set $dest (i32.add (local.get $dest) (i32.const 1)))          ;; Decrement size.
        (local.set $size (i32.sub (local.get $size) (i32.const 1)))
        (br $copy)                                                           ;; Repeat the loop.
      )
    )
  )
  
  ;; Function to concatenate two strings.
  (func $_strcat (param $str1 i32) (param $str2 i32) (result i32)
    (local $len1 i32) ;; Length of the first string.
    (local $len2 i32) ;; Length of the second string.
    (local $result i32) ;; Pointer to the new concatenated string.
    ;; Calculate the length of the first string.
    (local.set $len1 (call $_strlen (local.get $str1)))
    ;; Calculate the length of the second string.
    (local.set $len2 (call $_strlen (local.get $str2)))
    ;; Allocate memory for the concatenated string using _alloc_str.
    (local.set $result (call $_alloc_str (i32.add (local.get $len1) (local.get $len2))))
    ;; Copy the first string into the allocated memory.
    (call $_memcpy (local.get $str1) (local.get $result) (local.get $len1))
    ;; Copy the second string immediately after the first string in the allocated memory.
    (call $_memcpy (local.get $str2) (i32.add (local.get $result) (local.get $len1)) (local.get $len2)) ;; Include null terminator.
    ;; Return the pointer to the concatenated string.
    (local.get $result)
  )
  
  ;; Function to swap the first two values on the stack.
  (func $_swap (param $a i32) (param $b i32) (result i32 i32)
    (local.get $b)
    (local.get $a)
  )
  
  ;; Function to repeat a string a given number of times
  (func $_repeat_string (param $str i32) (param $count i32) (result i32)
    (local $result i32)                                                      ;; Pointer to the resulting string
    (local $str_len i32)                                                     ;; Length of the input string
    (local $total_len i32)                                                   ;; Total length of the resulting string
    (local $temp_dest i32)                                                   ;; Temporary pointer for destination
    (local.set $str_len (call $_strlen (local.get $str)))
    (local.set $total_len (i32.mul (local.get $str_len) (local.get $count)))
    (local.set $result (call $_alloc_str (local.get $total_len)))
    (local.set $temp_dest (local.get $result))
    (block $exit_loop
      (loop $repeat_loop
        (br_if $exit_loop (i32.eqz (local.get $count)))
        (call $_memcpy (local.get $str) (local.get $temp_dest) (local.get $str_len))
        (local.set $temp_dest (i32.add (local.get $temp_dest) (local.get $str_len)))
        (local.set $count (i32.sub (local.get $count) (i32.const 1)))
        (br $repeat_loop)
      )
    )
    (local.get $result)
  )
  
  (func $_int2string (param $var0 i32) (result i32)
    (local $var2 i32)
    (local $var3 i32)
    (local $var4 i32)
    (local $temp0 i32)
    (local $temp1 i32)
    (local.get $var0)
    (i32.const 0)
    (i32.eq)
    (if
      (then
        (i32.const 0)
        (return)
      )
    )
    (i32.const 2)
    (local.set $var2)
    (i32.const 0)
    (local.set $var3)
    (local.get $var0)
    (i32.const 0)
    (i32.lt_s)
    (if
      (then
        (i32.const 1)
        (local.set $var3)
        (local.get $var0)
        (i32.const 0)
        (i32.const 1)
        (i32.sub)
        (i32.mul)
        (local.set $var0)
      )
    )
    (i32.const 13)
    (local.set $var4)
    (block $exit1
      (loop $loop1
        (local.get $var0)
        (i32.const 0)
        (i32.gt_s)
        (i32.eqz)
        (br_if $exit1)
        (i32.const 2)
        call $_alloc_str
        (local.set $temp0)
        (local.get $temp0)
        (local.get $var2)
        (local.get $var0)
        (i32.const 10)
        (i32.rem_s)
        (i32.add)
        (i32.load8_u)
        i32.store8
        (local.get $temp0)
        (local.get $var4)
        call $_strcat
        (local.set $var4)
        (local.get $var0)
        (i32.const 10)
        (i32.div_s)
        (local.set $var0)
        (br $loop1)
      )
    )
    (local.get $var3)
    (if
      (then
        (i32.const 2)
        call $_alloc_str
        (local.set $temp1)
        (local.get $temp1)
        (i32.const 45)
        i32.store8
        (local.get $temp1)
        (local.get $var4)
        call $_strcat
        (local.set $var4)
      )
    )
    (local.get $var4)
  )
  
  (func $_str_cmp (param $lhs i32) (param $rhs i32) (result i32)
    (local $len1 i32)
    (local $len2 i32)
    (local.set $len1 (call $_strlen (local.get $lhs)))
    (local.set $len2 (call $_strlen (local.get $rhs)))
    (i32.ne (local.get $len1) (local.get $len2))
    (if (then
      (return (i32.const 0))
    ))
    (block $exit
      (loop $compare
        (i32.eqz (local.get $len1))
        (br_if $exit)
        (i32.load8_u (local.get $lhs))
        (i32.load8_u (local.get $rhs))
        (i32.ne)
        (if (then
          (return (i32.const 0))
        ))
        (local.set $lhs (i32.add (local.get $lhs) (i32.const 1)))
        (local.set $rhs (i32.add (local.get $rhs) (i32.const 1)))
        (local.set $len1 (i32.sub (local.get $len1) (i32.const 1)))
        (br $compare)
      )
    )
    (i32.const 1)
  )
  
  (func $ParallelStreams (result i32)
    ;; Variables
    (local $var1 i32)                                                          ;; Variable: a1
    (local $var2 i32)                                                          ;; Variable: a2
    (local $var3 i32)                                                          ;; Variable: a3
    (local $var4 i32)                                                          ;; Variable: a4
    (local $var5 i32)                                                          ;; Variable: b1
    (local $var6 i32)                                                          ;; Variable: b2
    (local $var7 i32)                                                          ;; Variable: b3
    (local $var8 i32)                                                          ;; Variable: b4
    (local $var9 i32)                                                          ;; Variable: c1
    (local $var10 i32)                                                         ;; Variable: c2
    (local $var11 i32)                                                         ;; Variable: c3
    (local $var12 i32)                                                         ;; Variable: c4
    (local $var13 i32)                                                         ;; Variable: i
    
    ;; Temp Variables
    
    (i32.const 1)                                                              ;; Put a 1 on the stack
    (local.set $var1)                                                          ;; Set var 'a1' from stack
    (i32.const 2)                                                              ;; Put a 2 on the stack
    (local.set $var2)                                                          ;; Set var 'a2' from stack
    (i32.const 3)                                                              ;; Put a 3 on the stack
    (local.set $var3)                                                          ;; Set var 'a3' from stack
    (i32.const 4)                                                              ;; Put a 4 on the stack
    (local.set $var4)                                                          ;; Set var 'a4' from stack
    (i32.const 5)                                                              ;; Put a 5 on the stack
    (local.set $var5)                                                          ;; Set var 'b1' from stack
    (i32.const 6)                                                              ;; Put a 6 on the stack
    (local.set $var6)                                                          ;; Set var 'b2' from stack
    (i32.const 7)                                                              ;; Put a 7 on the stack
    (local.set $var7)                                                          ;; Set var 'b3' from stack
    (i32.const 8)                                                              ;; Put a 8 on the stack
    (local.set $var8)                                                          ;; Set var 'b4' from stack
    (i32.const 9)                                                              ;; Put a 9 on the stack
    (local.set $var9)                                                          ;; Set var 'c1' from stack
    (i32.const 10)                                                             ;; Put a 10 on the stack
    (local.set $var10)                                                         ;; Set var 'c2' from stack
    (i32.const 11)                                                             ;; Put a 11 on the stack
    (local.set $var11)                                                         ;; Set var 'c3' from stack
    (i32.const 12)                                                             ;; Put a 12 on the stack
    (local.set $var12)                                                         ;; Set var 'c4' from stack
    (i32.const 0)                                                              ;; Put a 0 on the stack
    (local.set $var13)                                                         ;; Set var 'i' from stack
    (block $exit1                                                              ;; Outer block for breaking while loop.
      (loop $loop1                                                             ;; Inner loop for continuing while.
        ;; WHILE Test condition...
        (local.get $var13)                                                         ;; Place var 'i' onto stack
        (i32.const 3000000)                                                        ;; Put a 3000000 on the stack
        (i32.lt_s)                                                                 ;; Stack2 < Stack1
        (i32.eqz)                                                                  ;; Invert the result of the test condition.
        (br_if $exit1)                                                             ;; If condition is false (0), exit the loop
        ;; WHILE Loop body...
        (local.get $var1)                                                          ;; Place var 'a1' onto stack
        (local.get $var13)                                                         ;; Place var 'i' onto stack
        (i32.const 1)                                                              ;; Put a 1 on the stack
        (i32.add)                                                                  ;; Stack2 + Stack1
        (i32.add)                                                                  ;; Stack2 + Stack1
        (local.set $var1)                                                          ;; Set var 'a1' from stack
        (local.get $var2)                                                          ;; Place var 'a2' onto stack
        (local.get $var13)                                                         ;; Place var 'i' onto stack
        (i32.const 2)                                                              ;; Put a 2 on the stack
        (i32.add)                                                                  ;; Stack2 + Stack1
        (i32.add)                                                                  ;; Stack2 + Stack1
        (local.set $var2)                                                          ;; Set var 'a2' from stack
        (local.get $var3)                                                          ;; Place var 'a3' onto stack
        (local.get $var13)                                                         ;; Place var 'i' onto stack
        (i32.const 3)                                                              ;; Put a 3 on the stack
        (i32.add)                                                                  ;; Stack2 + Stack1
        (i32.add)                                                                  ;; Stack2 + Stack1
        (local.set $var3)                                                          ;; Set var 'a3' from stack
        (local.get $var4)                                                          ;; Place var 'a4' onto stack
        (local.get $var13)                                                         ;; Place var 'i' onto stack
        (i32.const 4)                                                              ;; Put a 4 on the stack
        (i32.add)                                                                  ;; Stack2 + Stack1
        (i32.add)                                                                  ;; Stack2 + Stack1
        (local.set $var4)                                                          ;; Set var 'a4' from stack
        (local.get $var5)                                                          ;; Place var 'b1' onto stack
        (i32.const 2)                                                              ;; Put a 2 on the stack
        (i32.mul)                                                                  ;; Stack2 * Stack1
        (local.set $var5)                                                          ;; Set var 'b1' from stack
        (local.get $var6)                                                          ;; Place var 'b2' onto stack
        (i32.const 3)                                                              ;; Put a 3 on the stack
        (i32.mul)                                                                  ;; Stack2 * Stack1
        (local.set $var6)                                                          ;; Set var 'b2' from stack
        (local.get $var7)                                                          ;; Place var 'b3' onto stack
        (i32.const 5)                                                              ;; Put a 5 on the stack
        (i32.mul)                                                                  ;; Stack2 * Stack1
        (local.set $var7)                                                          ;; Set var 'b3' from stack
        (local.get $var8)                                                          ;; Place var 'b4' onto stack
        (i32.const 7)                                                              ;; Put a 7 on the stack
        (i32.mul)                                                                  ;; Stack2 * Stack1
        (local.set $var8)                                                          ;; Set var 'b4' from stack
        (local.get $var5)                                                          ;; Place var 'b1' onto stack
        (i32.const 1000)                                                           ;; Put a 1000 on the stack
        (i32.rem_s)                                                                ;; Stack2 % Stack1
        (local.set $var5)                                                          ;; Set var 'b1' from stack
        (local.get $var6)                                                          ;; Place var 'b2' onto stack
        (i32.const 1000)                                                           ;; Put a 1000 on the stack
        (i32.rem_s)                                                                ;; Stack2 % Stack1
        (local.set $var6)                                                          ;; Set var 'b2' from stack
        (local.get $var7)                                                          ;; Place var 'b3' onto stack
        (i32.const 1000)                                                           ;; Put a 1000 on the stack
        (i32.rem_s)                                                                ;; Stack2 % Stack1
        (local.set $var7)                                                          ;; Set var 'b3' from stack
        (local.get $var8)                                                          ;; Place var 'b4' onto stack
        (i32.const 1000)                                                           ;; Put a 1000 on the stack
        (i32.rem_s)                                                                ;; Stack2 % Stack1
        (local.set $var8)                                                          ;; Set var 'b4' from stack
        (local.get $var9)                                                          ;; Place var 'c1' onto stack
        (local.get $var5)                                                          ;; Place var 'b1' onto stack
        (i32.add)                                                                  ;; Stack2 + Stack1
        (local.set $var9)                                                          ;; Set var 'c1' from stack
        (local.get $var10)                                                         ;; Place var 'c2' onto stack
        (local.get $var6)                                                          ;; Place var 'b2' onto stack
        (i32.add)                                                                  ;; Stack2 + Stack1
        (local.set $var10)                                                         ;; Set var 'c2' from stack
        (local.get $var11)                                                         ;; Place var 'c3' onto stack
        (local.get $var7)                                                          ;; Place var 'b3' onto stack
        (i32.add)                                                                  ;; Stack2 + Stack1
        (local.set $var11)                                                         ;; Set var 'c3' from stack
        (local.get $var12)                                                         ;; Place var 'c4' onto stack
        (local.get $var8)                                                          ;; Place var 'b4' onto stack
        (i32.add)                                                                  ;; Stack2 + Stack1
        (local.set $var12)                                                         ;; Set var 'c4' from stack
        (local.get $var13)                                                         ;; Place var 'i' onto stack
        (i32.const 1)                                                              ;; Put a 1 on the stack
        (i32.add)                                                                  ;; Stack2 + Stack1
        (local.set $var13)                                                         ;; Set var 'i' from stack
        ;; WHILE start next loop.
        (br $loop1)                                                                ;; Jump back to the start of the loop
      )                                                                        ;; End loop
    )                                                                          ;; End block
    (local.get $var1)                                                          ;; Place var 'a1' onto stack
    (local.get $var2)                                                          ;; Place var 'a2' onto stack
    (i32.add)                                                                  ;; Stack2 + Stack1
    (local.get $var3)                                                          ;; Place var 'a3' onto stack
    (i32.add)                                                                  ;; Stack2 + Stack1
    (local.get $var4)                                                          ;; Place var 'a4' onto stack
    (i32.add)                                                                  ;; Stack2 + Stack1
    (local.get $var5)                                                          ;; Place var 'b1' onto stack
    (i32.add)                                                                  ;; Stack2 + Stack1
    (local.get $var6)                                                          ;; Place var 'b2' onto stack
    (i32.add)                                                                  ;; Stack2 + Stack1
    (local.get $var7)                                                          ;; Place var 'b3' onto stack
    (i32.add)                                                                  ;; Stack2 + Stack1
    (local.get $var8)                                                          ;; Place var 'b4' onto stack
    (i32.add)                                                                  ;; Stack2 + Stack1
    (local.get $var9)                                                          ;; Place var 'c1' onto stack
    (i32.add)                                                                  ;; Stack2 + Stack1
    (local.get $var10)                                                         ;; Place var 'c2' onto stack
    (i32.add)                                                                  ;; Stack2 + Stack1
    (local.get $var11)                                                         ;; Place var 'c3' onto stack
    (i32.add)                                                                  ;; Stack2 + Stack1
    (local.get $var12)                                                         ;; Place var 'c4' onto stack
    (i32.add)                                                                  ;; Stack2 + Stack1
  )                                                                          ;; END 'ParallelStreams' function definition.
  
  (export "ParallelStreams" (func $ParallelStreams))
  
)                                                                          ;; END program module
