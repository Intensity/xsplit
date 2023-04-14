# xsplit: Split and reassemble using XOR and one time pads

- xsplit and xassem utilities

To build and install:

```bash
  $ make
  # make install
```

A `PREFIX` may be provided to the `make` argument.

## Usage

```
  $ xsplit input_file        output_share1 output_share2 # ...
  $ xassem input_reassembled output_share1 output_share2 # ...
  $ cmp input_file input_reassembled
```

Standard input and output can be referenced with a "-" argument.
