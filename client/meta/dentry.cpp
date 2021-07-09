#include "dentry.hpp"

extern std::shared_ptr<rados_io> meta_pool;

dentry::dentry(uuid ino, bool mkdir) : this_ino(ino)
{
	if(mkdir){
		global_logger.log(dentry_ops, "Called dentry(" + uuid_to_string(ino) +") from mkdir");
	} else {
		global_logger.log(dentry_ops, "Called dentry(" + uuid_to_string(ino) +")");
		unique_ptr<char[]> raw_data = std::make_unique<char[]>(MAX_DENTRY_OBJ_SIZE);
		try {
			meta_pool->read(obj_category::DENTRY, uuid_to_string(ino), raw_data.get(), MAX_DENTRY_OBJ_SIZE, 0);
			this->deserialize(raw_data.get());
		} catch(rados_io::no_such_object &e){
			throw std::runtime_error("Dentry Corrupted: inode number " + uuid_to_string(ino));
		}
	}
}

void dentry::add_child(const std::string &filename, uuid ino){
	global_logger.log(dentry_ops, "Called dentry.add_child()");
	global_logger.log(dentry_ops, "file : " + filename + " inode number : " + uuid_to_string(ino));

	auto ret = this->child_list.insert(std::make_pair(filename, ino));
	if(!ret.second) {
		global_logger.log(dentry_ops, "Replace file with new ino");
		ret.first.value() = ino;
	}
}

void dentry::delete_child(const std::string &filename) {
	global_logger.log(dentry_ops, "Called dentry.delete_child()");
	global_logger.log(dentry_ops, "file : " + filename);

	auto it = this->child_list.find(filename);
	if(it != this->child_list.end()) {
		this->child_list.erase(it);
	} else {
		global_logger.log(dentry_ops, "delete_child called for nonexistent file");
	}
}

unique_ptr<char[]> dentry::serialize()
{
	global_logger.log(dentry_ops, "Called dentry.serialize()");
	size_t child_num = this->child_list.size();
	size_t raw_size = sizeof(size_t) + (child_num) * sizeof(int) + (this->get_total_name_length()) + (child_num)*sizeof(uuid);
	unique_ptr<char[]> raw = std::make_unique<char[]>(raw_size + 1);
	char *pointer = raw.get();

	memcpy(pointer, &(child_num), sizeof(size_t));
	pointer += sizeof(size_t);

	for(auto & it : this->child_list){
		/* serialiize name length */
		int name_length = static_cast<int>(it.first.length());
		memcpy(pointer, &(name_length), sizeof(int));
		pointer += sizeof(int);

		/* serialize name */
		memcpy(pointer, it.first.data(), static_cast<size_t>(name_length));
		pointer += name_length;

		/* serialize ino */
		memcpy(pointer, &(it.second), sizeof(uuid));
		pointer += sizeof(uuid);
	}

	return raw;
}

void dentry::deserialize(char *raw)
{
	global_logger.log(dentry_ops, "Called dentry.deserialize()");

	char *pointer = raw;
	size_t child_num;
	memcpy(&(child_num), pointer, sizeof(size_t));
	pointer = pointer + sizeof(size_t);

	global_logger.log(dentry_ops, "dentry child num : " + std::to_string(child_num));

	for(int i = 0; i < child_num; i++){
		int name_length;
		memcpy(&name_length, pointer, sizeof(int));
		pointer += sizeof(int);

		unique_ptr<char[]> name = std::make_unique<char[]>(static_cast<size_t>(name_length) + 1);
		memcpy(name.get(), pointer, static_cast<size_t>(name_length));
		pointer += name_length;

		uuid ino{};
		memcpy(&ino, pointer, sizeof(uuid));
		pointer += sizeof(uuid);

		this->child_list.insert(std::pair<std::string, uuid>(name.get(), ino));
		global_logger.log(dentry_ops, "name_length : " + std::to_string(name_length) + "child name : " + std::string(name.get()) + " child ino : " + uuid_to_string(ino));
	}

}

void dentry::sync()
{
	global_logger.log(dentry_ops,"Called dentry.sync()");
	size_t child_num = this->child_list.size();
	size_t raw_size = sizeof(size_t) + (child_num) * sizeof(int) + (this->get_total_name_length()) + (child_num)*sizeof(uuid);
	unique_ptr<char[]> raw = this->serialize();
	meta_pool->write(obj_category::DENTRY, uuid_to_string(this->this_ino), raw.get(), raw_size, 0);
}

uuid dentry::get_child_ino(const std::string& child_name)
{
	global_logger.log(dentry_ops, "Called dentry.get_child_ino(" + child_name + ")");

	auto ret = child_list.find(child_name);
	if(ret == child_list.end())
		return nil_uuid();
	else
		return ret->second;
}

void dentry::fill_filler(void *buffer, fuse_fill_dir_t filler)
{
	filler(buffer, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
	filler(buffer, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
	for (auto & it : this->child_list)
		filler(buffer, it.first.c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
}

uint64_t dentry::get_child_num() {
	return this->child_list.size();
}

uint64_t dentry::get_total_name_length() {
	uint64_t total_name_length = 0;

	for(auto& ret : this->child_list){
		total_name_length += ret.first.size();
	}

	return total_name_length;
}
