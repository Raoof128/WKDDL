# Security Policy

## Supported Versions
This project is a lab and is not intended for production deployment. Security fixes are provided on a best-effort basis.

## Reporting a Vulnerability
- Email `security@localhost` with a detailed description and reproduction steps.
- Do not disclose issues publicly until a fix or mitigation is published.
- For kernel-level issues, include WinDbg output and crash dumps if available.

## Safe Handling
- Test only inside virtual machines with snapshots.
- Do not submit or store private keys, signing certificates, or secrets in the repository.
- Remove test-signed drivers and disable test-signing mode after experiments.

