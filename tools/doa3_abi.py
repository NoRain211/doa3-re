"""ABI-visible state comparison for private DOA3 equivalence harnesses."""

PRESERVED = ("esp", "ebx", "esi", "edi", "ebp")


def compare(memory, reference, registers, expected, *, stack_bottom, entry_esp,
            return_bits=0, st0=None, expected_st0=None):
    """Compare RAM except [stack_bottom, entry_esp), the dead callee stack.

    entry_esp points at the original return-address slot, after the caller
    placed arguments. The return slot, arguments and all non-stack RAM remain
    observable. return_bits is 8 for AL, 32 for EAX, or 0 for void/x87; pass
    normalized ST0 values for a floating return. Volatile registers are omitted
    only after the harness owner audits the callers for extra dependencies.
    """
    if len(memory) != len(reference):
        raise ValueError("RAM extents differ")
    if not 0 <= stack_bottom <= entry_esp <= len(memory) - 4:
        raise ValueError("invalid stack bounds")
    if return_bits not in (0, 8, 16, 32):
        raise ValueError("invalid integer return width")
    count, first = 0, []
    for lo, hi in ((0, stack_bottom), (entry_esp, len(memory))):
        for start in range(lo, hi, 4096):
            end = min(start + 4096, hi)
            if memory[start:end] == reference[start:end]:
                continue
            for address in range(start, end):
                if memory[address] != reference[address]:
                    count += 1
                    if len(first) < 12:
                        first.append(address)
    differences = {name: (registers[name], expected[name])
                   for name in PRESERVED if registers[name] != expected[name]}
    if return_bits:
        mask = (1 << return_bits) - 1
        if registers["eax"] & mask != expected["eax"] & mask:
            differences["return"] = (registers["eax"] & mask, expected["eax"] & mask)
    if st0 != expected_st0:
        differences["st0"] = (st0, expected_st0)
    return count, first, differences
