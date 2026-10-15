Grammars: 28. Pooled over all of them:

- model decode: **27.2 ms/token** (4 threads)
- grammar with the full mask every step (`grammar_first`, e.g. speculative draft sampling): **35.6 ms/token** = 131% of decode time
- grammar on the default resample path (`common_sampler_sample`): **1.5 ms/token** = 6% of decode time
- steps on the resample path that needed the full mask: **181 / 3544 = 5.1%** (per grammar: median 5%, min 0%, max 64%)
- sampler chain (top-k 40, temp, dist) on the resample path: 0.2 ms/token

| grammar | tokens (first / resample) | full-mask steps on resample path | decode ms/tok | grammar ms/tok, full mask every step | grammar ms/tok, resample path | chain ms/tok |
|---|---|---|---|---|---|---|
| gbnf__arithmetic | 256 / 256 | 23 (9%) | 27.2 | 37.2 | 2.8 | 0.2 |
| gbnf__c | 5 / 5 | 2 (40%) | 22.8 | 25.8 | 9.6 | 0.2 |
| gbnf__chess | 256 / 256 | 20 (8%) | 24.7 | 27.5 | 1.9 | 0.2 |
| gbnf__english | 113 / 113 | 0 (0%) | 24.8 | 83.9 | 0.1 | 0.1 |
| gbnf__japanese | 111 / 111 | 0 (0%) | 22.6 | 34.4 | 0.1 | 0.2 |
| gbnf__json | 83 / 114 | 1 (1%) | 24.3 | 37.9 | 0.3 | 0.2 |
| gbnf__json_arr | 256 / 207 | 10 (5%) | 23.9 | 37.6 | 1.4 | 0.2 |
| gbnf__list | 256 / 256 | 5 (2%) | 23.7 | 40.1 | 0.9 | 0.1 |
| jsb__Github_easy__o10514 | 75 / 63 | 4 (6%) | 23.6 | 30.6 | 1.3 | 0.1 |
| jsb__Github_easy__o11173 | 70 / 93 | 1 (1%) | 28.1 | 30.6 | 0.5 | 0.2 |
| jsb__Github_hard__o14513 | 256 / 238 | 6 (3%) | 28.4 | 30.0 | 0.9 | 0.2 |
| jsb__Github_hard__o21391 | 256 / 256 | 22 (9%) | 30.2 | 27.5 | 2.4 | 0.2 |
| jsb__Github_medium__o12610 | 92 / 92 | 0 (0%) | 26.2 | 31.1 | 0.0 | 0.1 |
| jsb__Github_medium__o20530 | 19 / 19 | 1 (5%) | 24.9 | 25.8 | 2.5 | 0.2 |
| jsb__Github_trivial__o10076 | 23 / 27 | 2 (7%) | 22.1 | 31.8 | 2.7 | 0.2 |
| jsb__Github_trivial__o21828 | 11 / 11 | 4 (36%) | 20.5 | 18.4 | 7.1 | 0.2 |
| jsb__Github_ultra__o17072 | 78 / 202 | 4 (2%) | 34.4 | 66.7 | 1.4 | 0.2 |
| jsb__Github_ultra__o21135 | 256 / 256 | 8 (3%) | 30.7 | 25.9 | 0.7 | 0.1 |
| jsb__Glaiveai2K__book_flight_c933065e | 61 / 61 | 0 (0%) | 24.0 | 31.9 | 0.0 | 0.2 |
| jsb__Glaiveai2K__calculate_area_002918bf | 53 / 52 | 1 (2%) | 27.0 | 28.2 | 0.9 | 0.2 |
| jsb__JsonSchemaStore__cloudformation | 14 / 14 | 9 (64%) | 29.0 | 23.4 | 16.1 | 0.3 |
| jsb__JsonSchemaStore__cluster | 256 / 116 | 35 (30%) | 33.2 | 36.7 | 8.4 | 0.2 |
| jsb__Kubernetes__kb_1037_Normalized | 18 / 20 | 1 (5%) | 24.0 | 31.2 | 1.7 | 0.2 |
| jsb__Kubernetes__kb_1043_Normalized | 68 / 256 | 12 (5%) | 30.6 | 32.8 | 1.3 | 0.2 |
| jsb__Snowplow__sp_157_Normalized | 114 / 114 | 0 (0%) | 23.9 | 41.3 | 0.1 | 0.1 |
| jsb__Snowplow__sp_189_Normalized | 256 / 256 | 0 (0%) | 27.0 | 37.8 | 0.1 | 0.1 |
| jsb__WashingtonPost__wp_109_Normalized | 26 / 32 | 2 (6%) | 21.9 | 26.2 | 1.8 | 0.1 |
| jsb__WashingtonPost__wp_113_Normalized | 60 / 48 | 8 (17%) | 27.2 | 29.7 | 3.8 | 0.2 |
