# Crypto Module Audit (SEC-01)

Read-only assessment of `src/xrCore/crypto/` — the engine's signing/verification primitives.
**No code was changed**: the algorithms are baked into the signature format, so swapping them
breaks verification of all existing signed content and cannot be validated without a coordinated
re-sign + runtime test. This documents what's there, the risk, and the safe path if it's ever
migrated.

## What's there

OpenSSL-backed, in `src/xrCore/crypto/`:

| Primitive | File | Reality |
|-----------|------|---------|
| **DSA** | `xr_dsa.{h,cpp}`, `xr_dsa_signer`, `xr_dsa_verifyer` | **1024-bit** key (`key_bit_length = 1024`), 160-bit `q` (`private_key_length = 20`). OpenSSL `dsa_st`. `sign()` / `verify()` over arbitrary data. |
| **Digest** | `xr_sha.{h,cpp}` | Class is named **`xr_sha256`** but it is **SHA-1**: it uses `SHA_CTX` / `SHA_Init` / `SHA_Update` / `SHA_Final` and a **20-byte** (160-bit) digest. SHA-256 would be `SHA256_CTX` / `SHA256_*` / 32 bytes. **The name is a misnomer.** |

Usage: DSA sign/verify of data blobs (content/asset/signature verification via
`xr_dsa_verifyer`). The signer path (`xr_dsa_signer`, `generate_params` under `DEBUG`) is the
authoring side.

## Findings

1. **`xr_sha256` is SHA-1 (misnomer).** Anyone reading the code expects SHA-256; it computes
   SHA-1. This is a correctness/clarity bug, not a behavioural one — but it actively misleads.
2. **SHA-1 is cryptographically broken for collision resistance** (SHAttered, 2017; chosen-prefix
   collisions since 2019). 
3. **DSA-1024 is legacy** — NIST SP 800-131A disallowed DSA < 2048-bit after 2013.

## Risk in context (threat model)

Severity depends entirely on *what the signature protects and against whom*:

- If verification is only against a **baked-in public key** to check **first-party content**
  (the typical X-Ray use — confirming official/packed data), an attacker would need to forge a
  DSA-1024/SHA-1 signature without the private key. SHA-1 collisions do **not** by themselves let
  you forge a signature for a *chosen* message against an existing public key — that needs the
  private key or a second-preimage break (not currently practical). So *direct* forgery risk is
  **low** in this model.
- The exposure rises if the signed data is **attacker-influenced** (e.g., a signer that signs
  user-supplied input, enabling collision-pair attacks) or if the **private key** is weakly
  protected. The `generate_params`/signer path is `DEBUG`-gated, which limits attack surface in
  shipped builds.

Net: **legacy primitives, low *immediate* exploitability in the baked-in-key verification model,
but they should not be carried into any new security-relevant feature.**

## Recommendations (do NOT apply unilaterally)

1. **Cheap + safe now (separate change, needs a build):** rename `xr_sha256` → `xr_sha1` (and the
   `//SHA_DIGEST_LENGTH` comment is correct — keep it) so the code stops lying about the
   algorithm. Pure clarity; no format change. *(Left undone here to avoid an un-runtime-tested
   rename across xrCore right after the CODE-01 churn; trivial to do in a focused pass.)*
2. **Migration (only with a plan):** moving to SHA-256 + a modern signature (ECDSA P-256 or
   RSA-2048+) **breaks every existing signature**. It requires: re-signing all signed content,
   a versioned signature format (so old + new verify during transition), and a coordinated
   release. This is a project decision, not a drive-by fix.
3. **Don't** introduce new uses of `xr_dsa`/`xr_sha256` for anything security-relevant; if a new
   feature needs integrity/auth, use a current primitive directly.

## Disposition

SEC-01 **audited and documented** (2026-06-06). The misnomer rename (rec. 1) applied
(2026-06-06): `xr_sha256` → `xr_sha1` across `xr_sha.h/.cpp`, `xr_dsa_signer.h/.cpp`,
`xr_dsa_verifyer.h`, `configs_dump_verifyer.h/.cpp` (8 sites). No behavioural change.
No code changed for rec. 2/3 — algo migration is compatibility-breaking and requires a
coordinated re-sign + release plan.
