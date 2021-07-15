#include "uuid.hpp"

uuid uuid_controller::alloc_new_uuid() {
	global_logger.log(inode_ops, "Called alloc_new_uuid()");
	return this->generator();
}

uint64_t uuid_controller::get_prefix_from_uuid(const uuid& id) {
	uint64_t prefix;
	const uint8_t* cursor = id.data;
	memcpy(&prefix, cursor, sizeof(uint64_t));

	return prefix;
}

uint64_t uuid_controller::get_postfix_from_uuid(const uuid& id){
	uint64_t postfix;
	const uint8_t* cursor = id.data + 8;
	memcpy(&postfix, cursor, sizeof(uint64_t));

	return postfix;
}

uuid uuid_controller::splice_prefix_and_postfix(const uint64_t prefix, const uint64_t postfix){
	uuid spliced_uuid{};
	uint8_t* cursor = spliced_uuid.data;
	memcpy(cursor, &prefix, sizeof(uint64_t));

	cursor = cursor + 8;
	memcpy(cursor, &postfix, sizeof(uint64_t));

	return spliced_uuid;
}

uuid get_root_ino(){
	global_logger.log(inode_ops, "Called get_root_ino()");
	/* root_ino = nil_uuid + 1 */
	uuid root_ino = nil_uuid();
	root_ino.data[15] = root_ino.data[15] + static_cast<uint8_t>(1);
	return root_ino;
}

std::string uuid_to_string(uuid id){
	return to_string(id);
}
