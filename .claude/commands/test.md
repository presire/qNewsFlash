# Test Project

Run manual tests for qNewsFlash.

## Usage
```
/test [type]
```

## Parameters
- `type`: Test type (manual, systemd, cron)

## Execution

### Manual Test
```bash
# Build first
cd build
./qNewsFlash --sysconf=../etc/qNewsFlash.json.in

# To exit: press 'q' or 'Q' then Enter
```

### Systemd Service Test
```bash
# Check service status
systemctl --user status qnewsflash.service

# View logs
journalctl --user -u qnewsflash.service -f

# Restart service
systemctl --user restart qnewsflash.service
```

### Cron Test (One-shot)
```bash
# Ensure autofetch = false in config
# Then run
./qNewsFlash --sysconf=/path/to/qNewsFlash.json
```

## Notes
- Requires valid qNewsFlash.json configuration
- API keys must be set in config file
- Test with Debug build for verbose output
