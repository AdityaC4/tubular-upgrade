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
  
  (func $SetAt (param $var0 i32) (param $var1 i32) (param $var2 i32) (result i32)
    ;; Variables
    
    ;; Temp Variables
    
    ;; Test condition for if.
    ;; Setup the && operation
    (local.get $var1)                                                          ;; Place var 'index' onto stack
    (i32.const 0)                                                              ;; Put a 0 on the stack
    (i32.ge_s)                                                                 ;; Stack2 >= Stack1
    (if (result i32)                                                           ;; Setup for && operator
      (then
        (local.get $var1)                                                          ;; Place var 'index' onto stack
        (local.get $var0)                                                          ;; Place var 'str' onto stack
        (call $_strlen)                                                            ;; Call _strlen for size()
        (i32.lt_s)                                                                 ;; Stack2 < Stack1
        (i32.const 0)                                                              ;; Put a zero on the stack for comparison)
        (i32.ne)                                                                   ;; Set any non-zero value to one.)
      )
      (else 
        (i32.const 0)                                                          ;; First clause of && was false.
      )
    )
    ;; End of && operation
    (if                                                                        ;; Execute code based on result of condition.
      (then                                                                      ;; 'then' block
        (local.get $var2)                                                          ;; Place var 'c' onto stack
        (local.get $var0)                                                          ;; Place var 'str' onto stack
        (local.get $var1)                                                          ;; Place var 'index' onto stack
        (i32.add)                                                                  ;; Compute address: base + index
        (call $_swap)                                                              ;; Swapping top 2 values on the stack to alight then for store.
        (i32.store8)                                                               ;; Store value at computed address
        (local.get $var0)                                                          ;; Place var 'str' onto stack
        (local.get $var1)                                                          ;; Place var 'index' onto stack
        (i32.add)                                                                  ;; Computer address: base + index
        (i32.load8_u)                                                              ;; Load byte at computed address
        (drop)                                                                     ;; Remove unneeded value from stack.
      )                                                                          ;; End 'then'
    )                                                                          ;; End 'if'
    (local.get $var0)                                                          ;; Place var 'str' onto stack
  )                                                                          ;; END 'SetAt' function definition.
  
  (export "SetAt" (func $SetAt))
  
)                                                                          ;; END program module
