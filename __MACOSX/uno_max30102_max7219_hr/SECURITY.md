# Security policy

This repository is an unauthenticated localhost prototype. It is not suitable for deployment on a public network or for real personal, medical, or custodial data.

The local server binds to `127.0.0.1` by default. It accepts a bearer header on the ESP32 request for gateway integration, but the sample server does not validate that token. Do not expose the server directly to a LAN or public network. For V2, put it behind a TLS gateway that validates device credentials and protects dashboard/API reads.

Do not change the bind address for a real deployment without first adding and reviewing authentication, authorization, encryption, audit logging, retention controls, and a threat model.

If you find a security issue, do not include real personal data in a report. Contact the repository owner privately through the GitHub profile associated with this project.
