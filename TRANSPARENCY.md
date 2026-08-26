# Generative AI transparency

This project follows the spirit of the
[NLnet policy on the use of generative AI](https://nlnet.nl/foundation/policies/generativeAI/):
use is permitted, but it must be disclosed and traceable.

## How generative AI is used here

| Area | LLM involvement | Human role |
|---|---|---|
| Documentation, README, English texts | Drafted with Anthropic Claude | Author reviews, corrects and approves every text before it is committed |
| Engineering decisions, architecture, component selection | None — decisions are the author's | Author |
| Firmware and hardware design files | Authored by the author; any LLM-assisted code will be marked in commit messages | Author designs, implements and tests |
| Measurement data and test results | Never generated — only real measurements are published | Author |

## Traceability

- Commits containing LLM-assisted content carry a `Co-Authored-By: Claude` trailer.
- Model used to date: Anthropic Claude (2026). Prompt logs for grant applications are
  maintained separately and disclosed to funders as required by their policies.

## Contact

Questions about any specific content's provenance: open an issue.
