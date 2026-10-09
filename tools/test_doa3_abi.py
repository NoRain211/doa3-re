import unittest

from doa3_abi import compare


class AbiGateTest(unittest.TestCase):
    def test_only_dead_stack_and_unused_register_bits_are_ignored(self):
        ref = bytearray(128)
        got = ref.copy()
        regs = dict(esp=100, ebx=2, esi=3, edi=4, ebp=5, eax=1, ecx=6, edx=7)
        changed = dict(regs, eax=0xffffff01, ecx=0, edx=0)
        got[32:96] = b'\xff' * 64
        kwargs = dict(stack_bottom=32, entry_esp=96, return_bits=8)
        self.assertEqual(compare(got, ref, changed, regs, **kwargs), (0, [], {}))
        for address in (0, 31, 96, 100, 127):
            got[address] = 1
        count, first, diff = compare(got, ref, changed, regs, **kwargs)
        self.assertEqual((count, first, diff), (5, [0, 31, 96, 100, 127], {}))
        changed.update(esp=104, ebx=0, esi=0, edi=0, ebp=0, eax=0)
        _, _, diff = compare(ref, ref, changed, regs, **kwargs)
        self.assertEqual(set(diff), {'esp', 'ebx', 'esi', 'edi', 'ebp', 'return'})
        kwargs['return_bits'] = 32
        self.assertIn('return', compare(ref, ref, dict(regs, eax=257), regs, **kwargs)[2])
        self.assertIn('st0', compare(ref, ref, regs, regs, st0=1.0,
                                    expected_st0=2.0, **kwargs)[2])
        with self.assertRaises(ValueError):
            compare(ref, ref, regs, regs, stack_bottom=100, entry_esp=96)


if __name__ == '__main__':
    unittest.main()
