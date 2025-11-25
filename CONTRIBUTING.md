# Contributing

Thanks for helping improve the Windows Kernel Driver Detection Lab! Contributions are welcome as long as safety remains the top priority.

## Ground Rules
- Only test changes inside isolated virtual machines with snapshots.
- Avoid experimental kernel hooks or unsupported APIs that may destabilize the OS.
- Keep code warnings-free (`/W4 /WX`) and prefer buffered I/O for kernel communication.
- Include clear documentation updates alongside code changes.

## How to Contribute
1. Fork the repository and create a feature branch.
2. Add or update tests/documentation relevant to your change.
3. Run available build steps (driver and client) in a VM.
4. Submit a pull request describing the change, risks, and testing performed.

## Code Style
- Kernel code: C with explicit NT status checks; avoid unchecked pointer arithmetic.
- User-mode code: Modern C++17, RAII where possible, and explicit error handling.
- Keep functions small and focused; prefer descriptive naming over abbreviations.

## Security and Disclosure
See `SECURITY.md` for reporting vulnerabilities. Never commit secrets, certificates, or private keys to the repository.

