import importlib.util
import json
from pathlib import Path
import sys

import pytest

HERE = Path(__file__).resolve().parent


@pytest.fixture
def cli(monkeypatch, tmp_path):
    spec = importlib.util.spec_from_file_location('protocol_policy', HERE / 'validate.py')
    policy = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(policy)
    rubric = tmp_path / 'rubric.json'
    rubric.write_text('{"comparison": {}}', encoding='utf-8')
    result = tmp_path / 'result.json'
    monkeypatch.setattr(sys, 'argv', ['validate.py', '--reference', str(tmp_path / 'reference'), '--candidate', str(tmp_path / 'candidate'), '--rubric', str(rubric), '--out', str(result)])
    return policy, result, rubric


def good():
    return {'passed': True, 'reason': 'valid', 'distance': 0.0, 'bound_fraction': 0.0}


def assert_fresh_failure(path):
    payload = path.read_bytes()
    assert payload.isascii()
    record = json.loads(payload.decode('utf-8'))
    assert record['passed'] is False
    assert record['distance'] is None and record['bound_fraction'] is None
    assert set(record) == {'passed', 'reason', 'distance', 'bound_fraction'}


@pytest.mark.parametrize('kind', [LookupError, AttributeError, AssertionError, ArithmeticError])
def test_unknown_comparison_exception_is_failure(cli, monkeypatch, kind):
    policy, result, _ = cli

    def explode(*args):
        raise kind('unexpected scientific comparison failure')

    monkeypatch.setattr(policy, 'evaluate', explode)
    assert policy.main() == 0
    assert_fresh_failure(result)


@pytest.mark.parametrize('bad_value', [float('nan'), float('inf'), object()])
def test_partial_pass_is_discarded_when_json_fails(cli, monkeypatch, bad_value):
    policy, result, _ = cli
    partial = dict(good(), distance=bad_value, reference={'passed': True})
    monkeypatch.setattr(policy, 'evaluate', lambda *args: partial)
    assert policy.main() == 0
    assert_fresh_failure(result)


def test_unprintable_exception_message_does_not_break_fallback(cli, monkeypatch):
    policy, result, _ = cli

    class BrokenMessage(Exception):
        def __str__(self):
            raise UnicodeError('cannot render this exception')

    def explode(*args):
        raise BrokenMessage()

    monkeypatch.setattr(policy, 'evaluate', explode)
    assert policy.main() == 0
    assert_fresh_failure(result)


def test_json_serializer_exception_is_failure(cli, monkeypatch):
    policy, result, _ = cli
    monkeypatch.setattr(policy, 'evaluate', lambda *args: good())

    def explode(*args, **kwargs):
        raise LookupError('serializer extension failed')

    monkeypatch.setattr(policy.json, 'dumps', explode)
    assert policy.main() == 0
    assert_fresh_failure(result)


def test_utf8_encoding_failure_is_inside_protection(cli, monkeypatch):
    policy, result, _ = cli
    monkeypatch.setattr(policy, 'evaluate', lambda *args: good())

    class BadText(str):
        def __add__(self, other):
            return self

        def encode(self, *args, **kwargs):
            raise UnicodeEncodeError('utf-8', '\ud800', 0, 1, 'forced encoding failure')

    monkeypatch.setattr(policy.json, 'dumps', lambda *args, **kwargs: BadText('{}'))
    assert policy.main() == 0
    assert_fresh_failure(result)


def test_unicode_and_lone_surrogate_are_ascii_escaped(cli, monkeypatch):
    policy, result, _ = cli
    expected = dict(good(), reason='lambda=λ; lone=\ud800')
    monkeypatch.setattr(policy, 'evaluate', lambda *args: expected)
    assert policy.main() == 0
    payload = result.read_bytes()
    assert payload.isascii()
    assert json.loads(payload.decode('utf-8')) == expected


def test_invalid_utf8_rubric_is_failure(cli):
    policy, result, rubric = cli
    rubric.write_bytes(bytes([255]))
    assert policy.main() == 0
    assert_fresh_failure(result)


@pytest.mark.parametrize('cancellation', [KeyboardInterrupt, SystemExit])
def test_cancellation_is_not_swallowed(cli, monkeypatch, cancellation):
    policy, result, _ = cli

    def cancel(*args):
        raise cancellation()

    monkeypatch.setattr(policy, 'evaluate', cancel)
    with pytest.raises(cancellation):
        policy.main()
    assert not result.exists()


def test_output_write_error_is_not_disguised(cli, monkeypatch):
    policy, result, _ = cli
    monkeypatch.setattr(policy, 'evaluate', lambda *args: good())
    result.mkdir()
    with pytest.raises(OSError):
        policy.main()
