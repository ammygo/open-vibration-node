# Generative AI transparency

This project follows the spirit of the
[NLnet policy on the use of generative AI](https://nlnet.nl/foundation/policies/generativeAI/):
use is permitted, but it must be disclosed and traceable.

## How generative AI is used here

| Area | LLM involvement | Human role |
|---|---|---|
| Documentation, README, English texts | Drafted with Anthropic Claude | Author reviews, corrects and approves every text before it is committed |
| Engineering decisions, architecture, component selection | None — decisions are the author's | Author |
| Hardware design files and bench test sketches | Authored by the author | Author |
| Bench-node firmware (`firmware/bench-node/`) | Written with Anthropic Claude from the author's specification; commits carry the `Co-Authored-By` trailer | Author specifies, reviews, tests on the hardware and approves before merge |
| Measurement data and test results | Never generated — only real measurements are published | Author |

## Traceability

- Commits containing LLM-assisted content carry a `Co-Authored-By: Claude` trailer.
- Model used to date: Anthropic Claude (2026). Prompt logs for grant applications are
  maintained separately and disclosed to funders as required by their policies.

## Contact

Questions about any specific content's provenance: open an issue.
