#ifndef FALLOUT_MAPPER_MP_TARGT_H_
#define FALLOUT_MAPPER_MP_TARGT_H_

namespace fallout {

typedef struct TargetSubNode {
    int pid;
    int tid;
    int field_8;
    int field_C;
    int field_10;
    int field_14;
    int field_18;
    int field_1C;
    int field_20;
    int field_24;
    struct TargetSubNode* next;
    int field_2C;
    int field_30;
    int field_34;
    int field_38;
    int field_3C;
    int field_40;
    int field_44;
    int field_48;
    int field_4C;
    int field_50;
    int field_54;
    int field_58;
    int field_5C;
    int field_60;
    int field_64;
    int field_68;
    int field_6C;
    int field_70;
    int field_74;
    int field_78;
    int field_7C;
    int field_80;
    int field_84;
    int field_88;
    int field_8C;
    int field_90;
    int field_94;
    int field_98;
    int field_9C;
} TargetSubNode;

void target_override_protection();
bool target_overriden();
void target_make_path(char* path, int pid);
int target_init();
int target_exit();
int target_header_save();
int target_header_load();
int target_save(int pid);
int target_load(int pid, TargetSubNode** subnode_ptr);
int target_find_free_subnode(TargetSubNode** subnode_ptr);
int target_new(int pid, int* tid_ptr);
int target_remove(int pid);
int target_remove_tid(int pid, int tid);
int target_remove_all();
int target_ptr(int pid, TargetSubNode** subnode_ptr);
int target_tid_ptr(int pid, int tid, TargetSubNode** subnode_ptr);
int pick_rot();
int target_pick_global_var(int* value_ptr);
int target_pick_map_var(int* value_ptr);
int target_pick_local_var(int* value_ptr);

} // namespace fallout

#endif /* FALLOUT_MAPPER_MP_TARGT_H_ */
