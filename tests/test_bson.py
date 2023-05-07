#!/usr/bin/env python
# vim: sts=4 sw=4 et

import pytest

import bson
from decimal import Decimal

from tll.test_util import Accum

@pytest.mark.parametrize("t,value",
        [('int8', -123),
        ('int16', -12323),
        ('int32', -123123),
        ('int64', -123123123123),
        ('uint8', 231),
        ('uint16', 54321),
        ('uint32', 123123123),
        ('double', 123.123),
        ('decimal128', Decimal("123.123")),
        ('string', 'text'),
        ('byte8', b'blob\0\0\0\0'),
        ('byte8, options.type: string', 'text'),
        ('"int8[4]"', [0, 1, 2]),
        ('"*int8"', [0, 1, 2]),
        ('"*int32"', [0, 1, 2]),
        ('Sub', {'s0': 10}),
        ('"Sub[4]"', [{'s0': 10}, {'s0': 20}]),
        ('"*Sub"', [{'s0': 10}, {'s0': 20}]),
        ('Union', {'i8': 10}),
        ('Union', {'s': 'string'}),
        ('"Union[4]"', [{'i8': 10}, {'s': 'string'}]),
        ('"*Union"', [{'i8': 10}, {'s': 'string'}]),
        ])
def test_field(context, t, value):
    r = Accum('direct://', name='raw', context=context)
    r.open()

    bvalue = value
    if isinstance(value, Decimal):
        bvalue = bson.Decimal128(value)

    scheme = f'''yamls://
- name: Sub
  fields:
    - {{name: s0, type: int8}}
- name: Data
  unions:
    Union: {{union: [{{name: i8, type: int8}}, {{name: s, type: string}}]}}
  id: 10
  fields:
    - {{name: f0, type: {t}}}
'''
    c = Accum('bson+direct://;direct.dump=text+hex;name=bson', master=r, scheme=scheme, context=context)
    c.open()

    assert c.state == c.State.Active

    c.post({'f0': value}, name='Data', seq=100)
    d = bson.decode(r.result[-1].data)
    assert d == {'_tll_name': 'Data', '_tll_seq': 100, 'f0': bvalue}

    d['_tll_seq'] = 200
    r.post(bson.encode(d))

    assert [(m.msgid, m.seq) for m in c.result] == [(10, 200)]

    assert c.unpack(c.result[-1]).as_dict() == {'f0': value}
