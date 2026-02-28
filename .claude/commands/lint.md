# Lint/Format Code

Run code formatting and static analysis on the project.

## Usage
```
/lint [fix]
```

## Parameters
- `fix`: Automatically fix formatting issues

## Execution

```bash
# Check for common issues
echo "Checking code style..."

# Check for tabs (should use spaces)
grep -rn $'\t' --include="*.cpp" --include="*.h" . && echo "WARNING: Found tabs in source files"

# Check for trailing whitespace
grep -rn ' $' --include="*.cpp" --include="*.h" . && echo "WARNING: Found trailing whitespace"

# Check for CRLF line endings
file *.cpp *.h | grep CRLF && echo "WARNING: Found CRLF line endings (should be LF)"

# If fix is requested
if [ "$1" = "fix" ]; then
  echo "Fixing formatting issues..."
  # Convert CRLF to LF
  sed -i 's/\r$//' *.cpp *.h 2>/dev/null
  # Remove trailing whitespace
  sed -i 's/[[:space:]]*$//' *.cpp *.h 2>/dev/null
  echo "Fixed!"
fi
```

## Notes
- Qt code style prefers 4-space indentation
- All source files must be UTF-8 encoded
- No trailing whitespace allowed
