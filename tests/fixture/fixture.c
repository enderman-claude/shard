/* Built as a plain shared object (no libc). The pointer below needs a
 * R_X86_64_RELATIVE relocation, so it exercises the ELF loader's patching. */
const char fixture_msg[] = "shard-elf-ok";
const char *const fixture_ptr = fixture_msg;
int fixture_answer(void) { return 42; }
