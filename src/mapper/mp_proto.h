#ifndef FALLOUT_MAPPER_MP_PROTO_H_
#define FALLOUT_MAPPER_MP_PROTO_H_

namespace fallout {

extern char* proto_builder_name;
extern bool can_modify_protos;

void init_mapper_protos();
const char* proto_wall_light_str(int flags);
int proto_pick_ai_packet(int* value);

} // namespace fallout

#endif /* FALLOUT_MAPPER_MP_PROTO_H_ */
