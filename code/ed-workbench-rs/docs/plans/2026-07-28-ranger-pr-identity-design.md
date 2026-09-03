# Ranger PR Identity Migration

## Scope

Update the current versions of open `QuantumBFS/quantum.harness` PRs #214,
#215, #216, #217, and #220. Preserve title wording and body edits made after
the earlier Rager rollout. PR #210 remains explicitly excluded.

## Naming

Perform a complete migration from `Rager` to `Ranger`:

- visible team names;
- headings and image alt text;
- lowercase media paths and URLs;
- supporting PR-body source material in the private #129 workbench.

The final five PR titles and bodies must contain neither `Rager` nor lowercase
`rager`.

## Bilingual Opening

Every in-scope PR begins with the same English quotation:

```markdown
> Do not go gentle into that good night,<br>
> Old age should burn and rave at close of day;<br>
> Rage, rage against the dying of the light.<br>
> —— [Dylan Thomas](https://www.poetryfoundation.org/poets/dylan-thomas), 「**Do Not Go Gentle into That Good Night**」
```

Immediately below it, add the requested Chinese version as a separate
blockquote:

```markdown
> 不要温和地走进那良夜，<br>
> 老年应当在日暮时燃烧咆哮；<br>
> 怒斥，怒斥光明的消逝。<br>
```

## Media

Reuse the approved five Interstellar-inspired original banners without
regeneration. Publish the same image bytes under:

- branch `media/ranger-pr-banners`;
- directory `assets/ranger/`.

Update every PR to reference the corresponding new raw URL. Do not delete the
old `media/rager-pr-banners` branch, because deletion is unnecessary and would
make prior revisions harder to audit.

## Content Preservation

Fetch every PR immediately before editing. Replace the complete opening quote
region rather than stacking a second Chinese quote on repeated runs. Reinsert
the correct PR-specific image for #215 and #220, whose latest externally
edited bodies no longer contain banners. Preserve everything following the
opening region byte-for-byte except the `Rager` to `Ranger` substitution.

## Verification

- all five titles contain `Ranger`;
- all five bodies begin with the exact English blockquote, Chinese blockquote,
  and unique Ranger image;
- neither `Rager` nor `rager` remains in any in-scope title or body;
- all five new raw URLs return HTTP 200 with `image/png`;
- the five current technical body suffixes remain unchanged apart from the
  explicit rename;
- PR #210 remains unchanged.
