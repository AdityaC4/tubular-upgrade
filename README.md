[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/HZI_uK1E)

# Project4

- Aditya Chaudhari

## Features Completed

I have completed extra credit:

1. Recursive Functions
2. Make the `:string` type casting work on integers
3. **Loop Unrolling Optimization** - NEW! Advanced compiler optimization feature

## Testing

For my testing I also added 3 new tests:

1. test 21: to check recursive function (Fibonacci sequence)
2. test 22: testing `:string` type casting
3. test 23: testing string compare using `==`
4. test 24: testing recursive function calls with strings (ReverseString)

### Loop Unrolling Tests

- **Comprehensive performance testing** with 4 specialized test cases
- **Multiple unroll factors**: 1x, 4x, 8x, 16x optimization levels
- **Performance improvements**: Up to 22% faster execution observed
- **Browser-based testing**: Interactive performance analysis

### Running Tests

```bash
./make test         # Run all tests (standard + loop unrolling)
./make clean        # Clean all generated files
./make clean-test   # Clean only test files
```

See [TESTING.md](TESTING.md) for complete testing documentation.
