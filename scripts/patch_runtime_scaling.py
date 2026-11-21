"""Build-local arithmetic fix for the pinned runtime processor."""
import hashlib
from pathlib import Path
import sys

SOURCE_SHA256 = "0a7f7216141e9126c8b4dcc4a53ab4d9a318fda82abd55ed4d5f3604e6d727d9"


def patch(source):
    source = source.replace('\r\n', '\n')
    if hashlib.sha256(source.encode()).hexdigest() != SOURCE_SHA256:
        raise ValueError('Runtime processor changed: review scaling patch')
    changes = {
        'int16_t value_mul = event->value * (int16_t)mul;':
            'int64_t value_mul = (int64_t)event->value * mul;',
        'int16_t scaled = value_mul / (int16_t)div;':
            'int64_t quotient = value_mul / div;\n    int16_t scaled = CLAMP(quotient, INT16_MIN, INT16_MAX);',
        '*state->remainder = value_mul - (scaled * (int16_t)div);':
            '/* Do not carry clipped motion into later reports. The ABI remainder is int16. */\n'
            '        *state->remainder = quotient == scaled\n'
            '            ? CLAMP(value_mul - quotient * div, INT16_MIN, INT16_MAX) : 0;',
    }
    for old, new in changes.items():
        if source.count(old) != 1:
            raise ValueError('Scaling hook missing or ambiguous')
        source = source.replace(old, new)
    return source


if __name__ == '__main__':
    Path(sys.argv[2]).write_text(patch(Path(sys.argv[1]).read_text()))
