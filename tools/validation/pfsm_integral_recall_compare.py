"""Independent validation of bounded-integral baseline diagnostic selection."""
import math

SOURCE = '3fa1123a16ab746f80f946dea55f4b9f5b633e48'
TIE = 128 * math.ulp(1.)


def validate_integral(attempt, records, old_count=None):
    errors = []
    d = attempt.get('integral', {})
    if (attempt.get('switch_reason') not in ('candidate_recall_grid_limit', 'candidate_recall_scan_limit') or
            d.get('source_revision') != SOURCE or d.get('implementation') != 'baseline' or attempt.get('v2')):
        errors.append('invalid integral backend/source')
    for key, expected in dict(fine_budget=64, coarse_budget=16, original_budget=16,
                              residual_budget=16, relaxed_budget=32, nnls_limit=205, tail_mass_epsilon=1e-4).items():
        if d.get(key) != expected:
            errors.append('changed integral '+key)
    if attempt.get('full_fallback'):
        if records:
            errors.append('partial integral fallback evidence')
        return errors
    fine = d.get('fine_bins', 0)
    if not 1 <= fine <= 2048 or d.get('coarse_bins') != (fine+1)//2:
        errors.append('integral bin limit')
    if old_count is not None and not 0 <= d.get('bank_columns', -1) <= 257*old_count:
        errors.append('integral bank limit')
    if attempt.get('recalled_indices') != attempt.get('retained_indices'):
        errors.append('integral recalled union differs')
    channels = [('normalized_fine_gain', 'fine_selected', 64),
                ('normalized_coarse_gain', 'coarse_selected', 16),
                ('original_score', 'original_selected', 16),
                ('residual_score', 'residual_selected', 16),
                ('relaxed_gain', 'relaxed_selected', 32)]
    union = set()
    for score, flag, count in channels:
        scored = []
        for row in records:
            v = row.get('integral', {})
            value = row.get(score) if score in ('original_score', 'residual_score') else v.get(score)
            if score == 'relaxed_gain' and (not v.get('relaxed_complete') or v.get('relaxed_uncertainty')):
                continue
            if value is not None:
                if not math.isfinite(value) or value < 0:
                    errors.append('invalid integral '+score)
                else:
                    scored.append((value, row['original_index']))
        scored.sort(key=lambda item: (-item[0], item[1]))
        cutoff = scored[min(count, len(scored))-1][0] if scored else math.inf
        selected = {i for value, i in scored if value >= cutoff-TIE}
        if selected != {r['original_index'] for r in records if r.get('integral', {}).get(flag)}:
            errors.append('incorrect integral '+flag)
        union |= selected
    totals = dict(coarse_evaluations=0, fine_evaluations=0, relaxed_evaluations=0)
    for row in records:
        v = row.get('integral', {})
        if not v:
            errors.append('missing integral candidate evidence')
            continue
        for key, limit in dict(coarse_evaluations=66, fine_evaluations=32, relaxed_evaluations=205,
                               candidate_cache_columns=32).items():
            value = v.get(key, -1)
            if type(value) is not int or not 0 <= value <= limit:
                errors.append('integral '+key+' limit')
            if key in totals:
                totals[key] += value
        released = v.get('released', [])
        if (released != sorted(set(released)) or len(released) > 2 or
                (old_count is not None and any(i < 0 or i >= old_count for i in released))):
            errors.append('invalid released coordinates')
        protected = bool(v.get('uncertainty')) or bool(v.get('relaxed_uncertainty'))
        if ((not v.get('integral_complete') or not v.get('relaxed_complete')) and not protected):
            errors.append('unaccounted integral failure')
        if protected:
            union.add(row['original_index'])
        if bool(row.get('protection_reason')) != protected:
            errors.append('inconsistent integral protection')
        if v.get('relaxed_complete'):
            size = old_count if old_count is not None else len(v.get('relaxed_null_shifts', []))
            for name, count in [('best', size+1), ('null', size)]:
                for kind in ('shifts', 'scales'):
                    values = v.get('relaxed_'+name+'_'+kind, [])
                    if len(values) != count or any(not math.isfinite(x) or (kind == 'scales' and x < 0) for x in values):
                        errors.append('invalid integral matched '+name)
    if any(d.get(key) != value for key, value in totals.items()):
        errors.append('integral evaluation totals')
    if union != set(attempt.get('retained_indices', [])):
        errors.append('incorrect integral union')
    return errors
