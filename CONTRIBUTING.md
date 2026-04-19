# Contributing to Voltis

Navigation: [README](README.md) · [Governance](GOVERNANCE.md) · [Code of Conduct](CODE_OF_CONDUCT.md) · [VEP template](veps/VEP-0001-template.md)

Thank you for contributing to Voltis. This project is a real compiler/language codebase and follows a strict review process.

## Ground rules

1. **PR-only workflow**: all code, docs, spec, and test changes must be submitted via pull request.
2. **No direct language forks under Voltis name**: divergent language definitions must use separate branding (see [GOVERNANCE.md](GOVERNANCE.md)).
3. **Spec and implementation must stay aligned**: language behavior changes must update docs/spec in the same PR unless intentionally staged and explicitly documented.
4. **Respect community standards**: all participation is governed by [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

## What to contribute

- parser/sema/backend improvements
- tests (positive and negative cases)
- documentation and spec clarifications
- tooling and build reliability improvements

## Development flow

1. Open an issue (bug report, feature request, or discussion).
2. Create a focused branch.
3. Implement changes with tests where applicable.
4. Update docs/spec if behavior is added or changed.
5. Submit a pull request with clear rationale and impact.

## Language change policy (VEP process)

Changes to syntax, semantics, conversion rules, type rules, built-ins, or backend-visible language behavior require a VEP.

1. Copy the template: [veps/VEP-0001-template.md](veps/VEP-0001-template.md)
2. Fill in problem, proposal, syntax, and impact.
3. Link the VEP in your PR.
4. Await maintainer review and acceptance before merge.

## Pull request checklist

- [ ] Scope is clear and focused.
- [ ] Tests added/updated for behavior changes.
- [ ] Docs/spec updated.
- [ ] Breaking changes are explicitly called out.
- [ ] Commit history is clean and reviewable.

## Review expectations

- Be precise and technical.
- Prefer small, incremental PRs over large rewrites.
- Address review comments fully or document why an alternative was chosen.
