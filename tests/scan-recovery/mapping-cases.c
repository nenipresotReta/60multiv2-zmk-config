int main(void) {
    uint32_t map[8];
    for (size_t i = 0; i < ARRAY_SIZE(map); i++) { map[i] = 0xdeadbeef; }
    assert(zmk_physical_layouts_get_position_map(0, 1, ARRAY_SIZE(map), map) == 3);
    assert(map[0] == 0);
    /* Both unmapped visible positions and the unused tail must be initialized. */
    for (size_t i = 1; i < ARRAY_SIZE(map); i++) { assert(map[i] == UINT32_MAX); }
    assert(zmk_physical_layouts_get_position_map(0, 0, ARRAY_SIZE(map), map) == 0);
    for (size_t i = 0; i < ARRAY_SIZE(map); i++) { assert(map[i] == i); }
    assert(zmk_physical_layouts_get_position_map(0, 1, 2, map) == -EINVAL);
    puts("PASS real layout mapper initializes unmatched positions and the full uint32_t tail");
    return 0;
}
