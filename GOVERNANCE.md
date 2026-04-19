# Voltis Governance

Navigation: [README](README.md) · [Contributing](CONTRIBUTING.md) · [Roadmap](ROADMAP.md) · [Spec](docs/spec/syntax.md)

## Authority and language identity

1. **The Voltis specification in this repository is authoritative.**
2. **Only this repository defines the language named "Voltis".**
3. Forks or derivative language projects must not present themselves as official Voltis.
4. A fork that changes language behavior must use distinct branding if it diverges from the official spec.

## Change control

1. All language/compiler changes must go through pull requests.
2. Direct pushes to protected branches are reserved for maintainers and release operations.
3. Language-surface changes (syntax, semantics, standard behavior) require a VEP proposal in `veps/`.
4. Merged changes update the spec and implementation together, or explicitly document staged rollout.

## Maintainer role

Maintainers are responsible for:

- language direction and release policy
- acceptance/rejection of VEPs
- consistency between implementation and specification
- repository quality, moderation, and final technical arbitration

## Decision process

1. Open an issue describing the problem or desired change.
2. Submit a PR (and VEP for language changes).
3. Receive maintainer review and required revisions.
4. Merge only after consensus or maintainer decision.

## Compatibility posture

Voltis is early-stage. Breaking changes may occur, but they must be:

- documented in spec and changelog context (as applicable)
- justified by correctness, safety, or long-term language coherence
- reflected in tests when behavior changes
