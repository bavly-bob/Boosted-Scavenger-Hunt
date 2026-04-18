# AI Integration Setup Guide

## Overview
The game now includes AI-powered clue generation using OpenRouter + elephant-alpha model. Clues are dynamically rephrased by the AI for more engaging gameplay.

## Environment Variables

To enable AI clue generation, set these environment variables **before** launching the game:

### PowerShell (Windows)
```powershell
$env:AI_API_KEY = 'sk-or-v1_YOUR_FULL_API_KEY'
$env:AI_MODEL = 'elephant-alpha'
```

### Command Prompt (Windows)
```cmd
set AI_API_KEY=sk-or-v1_YOUR_FULL_API_KEY
set AI_MODEL=elephant-alpha
```

### Bash/Linux/macOS
```bash
export AI_API_KEY='sk-or-v1_YOUR_FULL_API_KEY'
export AI_MODEL='elephant-alpha'
```

## Your API Key
Use the API key you provided:
```
sk-or-v1 48106599b3f560eb65438d16202efa636f5e437c29ce389dc9833c8757c00d06
```

Set it as:
```powershell
$env:AI_API_KEY = 'sk-or-v1'
$env:AI_MODEL = 'elephant-alpha'
```

Wait, I notice your API key has a space in it. That shouldn't be there. The actual key should be continuous without spaces. If the key is:
```
sk-or-v1_48106599b3f560eb65438d16202efa636f5e437c29ce389dc9833c8757c00d06
```
(with underscore replacing the space), then set it as:
```powershell
$env:AI_API_KEY = 'sk-or-v1_48106599b3f560eb65438d16202efa636f5e437c29ce389dc9833c8757c00d06'
$env:AI_MODEL = 'elephant-alpha'
```

## How It Works

1. **Without API Key**: Clues display as-is from the JSON level file
2. **With API Key**: Each clue is sent to OpenRouter's AI for rephrasing
   - Takes ~500-2000ms per clue (network latency)
   - Falls back to original clue if network fails
   - Respects game rate limits

## Customization

### Override API URL
If you want to use a different API endpoint:
```powershell
$env:AI_API_URL = 'https://api.openai.com/v1/chat/completions'
```

### Disable AI Temporarily
Simply don't set `AI_API_KEY` or set it to an empty string:
```powershell
$env:AI_API_KEY = ''
```

## Testing the Setup

Run the game and trigger a clue-revealing location:
- Collect coins (triggers coin-based clues)
- Step on a ClueTrigger tile (if defined in the level JSON)

Check console output to verify:
- `isEnabled() = true` means AI is active
- Network requests are logged (watch for HTTP/HTTPS calls)

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Clues not rephrased | Verify `AI_API_KEY` is set and non-empty |
| Network errors | Check internet connection; API key validity |
| Slow clue display | Network latency; normal for ~1-2s response time |
| Original clue shown | Network failed; check API key permissions |

## Security Notes

- **Never commit** API keys to version control
- Use separate API keys for dev/production
- Monitor API usage and set rate limits in OpenRouter dashboard
- Keys are read from environment only; not stored in code or config files
