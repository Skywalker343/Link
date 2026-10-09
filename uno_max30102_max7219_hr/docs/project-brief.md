# Portfolio project brief

## Project concept

This project explores a human-centered monitoring system for correctional facilities. It brings together wearable sensing, physiological signals, and digital management tools in a unified concept. The intended system translates measurements into concise digital information so staff can review a person's status and respond to a need for human attention.

The design goal is to explore supervision that is less intrusive and more operationally legible while balancing security operations with dignity and care. It is not a proposal to automate health or disciplinary decisions.

## Current code prototype

The files currently implement a narrower technical slice: an Arduino UNO reads IR samples from a fingertip MAX30102 sensor, estimates beat intervals, displays a symbolic heart-rate band on a MAX7219 matrix, and streams a small JSON record over USB serial to a local dashboard.

The garment, 24-hour wear, SpO₂, motion/activity sensing, wireless transport, native mobile app, staff accounts, and production data controls remain future design work. The local dashboard is a responsive web prototype; its sample fleet and review item are synthetic.

## Portfolio-ready project description

> CARE SIGNAL is a human-centered monitoring concept for correctional environments. This prototype connects a MAX30102 sensor node, symbolic LED feedback, a local data service, and a responsive staff dashboard. It explores how concise physiological data can support timely human review while making measurement limits, synthetic records, and privacy boundaries visible. The current implementation demonstrates heart-rate sensing and a serial-to-dashboard pipeline; continuous wearable monitoring and mobile/facility integrations are future work.

## Suggested presentation

- Show the full-system concept as a roadmap and label each unbuilt component clearly.
- Present the working Arduino-to-dashboard path as the implemented prototype.
- Use only synthetic device data in screenshots and portfolio videos.
- Explain that visual bands and review prompts are interface demonstrations, not health thresholds.
