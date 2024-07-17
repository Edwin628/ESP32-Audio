void printHeapInfo() {
    Serial.println("Heap info:");
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
}
