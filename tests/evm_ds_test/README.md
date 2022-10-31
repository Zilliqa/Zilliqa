# Running EVM DS Tests

## Install the Solidity Compiler

```
Use the instructions at https://docs.soliditylang.org/en/v0.8.9/installing-solidity.html#linux-packages
```

## Install Poetry

```
python3 -m pip install poetry
```

Running the tests

```
poetry run pytest
or with optional parameters:
- Filter on test case : -k <test name>
- For extra debug logging: --log-level=DEBUG
```

