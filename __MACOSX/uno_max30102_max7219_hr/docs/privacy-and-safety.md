# Privacy and safety notes

This project concerns monitoring in a custodial setting, where people have limited control over the environment and data collection can affect rights, dignity, and access to care. The code is therefore framed as a sensing-and-interface prototype, not a decision system.

- Do not collect real inmate or patient data with this prototype.
- Do not use a heart-rate estimate to infer intent, misconduct, emotional state, or a medical condition.
- Do not automate restraint, discipline, housing, privileges, or emergency decisions from a sensor value.
- Treat missing, stale, noisy, or implausible readings as unavailable. Ask a trained person to check the person and device.
- The sample UI's review threshold exists only to show a workflow. It is not calibrated to a population or clinical protocol.
- Before any real deployment, conduct privacy, security, accessibility, safety, and human-rights reviews with the people affected and qualified domain experts.

The serial service is unauthenticated and stores rows in a local SQLite file. It is intended for a localhost demo with synthetic data only.
