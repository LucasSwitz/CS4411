/* ctx_switch() saves the registers of the current process on its
 * stack and then the stack pointer in *old_sp.  It then sets the stack
 * pointer to new_sp and restores the registers previously saved.
 * ctx_start() is similar but instead of restoring registers it invokes
 * ctx_entry().
 */
extern void ctx_switch(address_t *old_sp, address_t new_sp);
extern void ctx_start(address_t *old_sp, address_t new_sp);
void ctx_entry(void);
