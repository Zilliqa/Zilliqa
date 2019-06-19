# Zilliqa Coding and Review Guidelines

1. All submitted **Pull Requests** are assumed to be ready for review.  
   If not, they must be marked with explicit  `[in progress]` or have "[WIP]" in title.

2. - **Pull requests** which fix a bug should have branch name starting with `fix/`
   - **Pull requests** which add a feature to the codebase should have branch name starting with `feature/`
   - **Pull requests** which are made by outside contributors should also contain their name.
   ```
   Example:
      john/fix/xxx
    ```

3. All **Pull Requests** which are not ready to be merged yet but are open for review should be marked with explicit  
   `on hold` or should be a draft.

4. - To build your code with clang-format, use `./build.sh style`
   - To build your code with clang-style, use `./build.sh linter`

5. Kindly go through `.clang-format` and `.clang-style` to see the format and checks enabled
