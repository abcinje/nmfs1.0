#include "dentry.hpp"

extern rados_io *meta_pool;

dentry::dentry(ino_t ino) : this_ino(ino)
{
	global_logger.log(dentry_ops, "Called dentry(" + std::to_string(ino) +")");
	unique_ptr<char[]> raw_data = std::make_unique<char[]>(MAX_DENTRY_OBJ_SIZE);
	try {
		meta_pool->read(obj_category::DENTRY, std::to_string(ino), raw_data.get(), MAX_DENTRY_OBJ_SIZE, 0);
		this->deserialize(raw_data.get());
	} catch(rados_io::no_such_object &e){
		throw std::runtime_error("Dentry Corrupted: inode number " + std::to_string(ino));
	}
}

dentry::dentry(ino_t ino, bool flag) : this_ino(ino)
{
	global_logger.log(dentry_ops, "Called dentry(" + std::to_string(ino) +") from mkdir");
	if(flag){
		this->child_num = 0;
		this->total_name_length = 0;
	} else {
		throw std::runtime_error("Something wrong in mkdir");
	}
}

void dentry::add_new_child(const std::string &filename, ino_t ino){
	global_logger.log(dentry_ops, "Called dentry.add_new_child()");
	global_logger.log(dentry_ops, "file : " + filename + " inode number : " + std::to_string(ino));

	this->child_list.insert(std::make_pair(filename, ino));
	this->child_num++;
	this->total_name_length += filename.length();
}

void dentry::delete_child(const std::string &filename) {
	global_logger.log(dentry_ops, "Called dentry.delete_child()");
	global_logger.log(dentry_ops, "file : " + filename);

	this->child_list.erase(filename);
	this->child_num--;
	this->total_name_length -= filename.length();
}

unique_ptr<char[]> dentry::serialize()
{
	global_logger.log(dentry_ops, "Called dentry.serialize()");
	size_t raw_size = sizeof(uint64_t) + (this->get_child_num()) * sizeof(int) + (this->get_total_name_legth()) + (this->get_child_num())*sizeof(ino_t) + 1;
	unique_ptr<char[]> raw = std::make_unique<char[]>(raw_size);
	char *pointer = raw.get();

	memset(pointer, '\0', raw_size);
	memcpy(pointer, &(this->child_num), sizeof(uint64_t));
	pointer += sizeof(uint64_t);

	for(auto it = this->child_list.begin(); it != this->child_list.end(); it++){
		/* serialiize name length */
		int name_length = static_cast<int>(it->first.length());
		memcpy(pointer, &(name_length), sizeof(int));
		pointer += sizeof(int);

		/* serialize name */
		memcpy(pointer, it->first.data(), name_length);
		pointer += name_length;

		/* serialize ino */
		memcpy(pointer, &(it->second), sizeof(ino_t));
		pointer += sizeof(ino_t);
	}

	return raw;
}

void dentry::deserialize(char *raw)
{
	global_logger.log(dentry_ops, "Called dentry.deserialize()");

	char *pointer = raw;

	memcpy(&(this->child_num), pointer, sizeof(uint64_t));
	pointer = pointer + sizeof(uint64_t);

	global_logger.log(dentry_ops, "dentry child num : " + std::to_string(this->child_num));

	this->total_name_length = 0;
	for(int i = 0; i < this->child_num; i++){
		int name_length;
		memcpy(&name_length, pointer, sizeof(int));
		pointer += sizeof(int);

		char* name=(char *)calloc(name_length + 1, sizeof(char));
		memcpy(name, pointer, name_length);
		pointer += name_length;

		ino_t ino;
		memcpy(&ino, pointer, sizeof(ino_t));
		pointer += sizeof(ino_t);

		this->child_list.insert(std::pair<std::string, ino_t>(name, ino));
		this->total_name_length += name_length;
		global_logger.log(dentry_ops, "name_length : " + std::to_string(name_length) + "child name : " + std::string(name) + " child ino : " + std::to_string(ino));
		free(name);
	}

}

void dentry::sync()
{
	global_logger.log(dentry_ops,"Called dentry.sync()");

	size_t raw_size = sizeof(uint64_t) + (this->child_num) * sizeof(int) + (this->total_name_length) + (this->child_num)*sizeof(ino_t) + 1;
	unique_ptr<char[]> raw = this->serialize();
	meta_pool->write(obj_category::DENTRY, std::to_string(this->this_ino), raw.get(), raw_size - 1, 0);
}

ino_t dentry::get_child_ino(std::string child_name)
{
	global_logger.log(dentry_ops, "Called dentry.get_child_ino(" + child_name + ")");

	std::map<std::string, ino_t>::iterator ret = child_list.find(child_name);
	if(ret == child_list.end())
		return -1;
	else
		return ret->second;
}

void dentry::fill_filler(void *buffer, fuse_fill_dir_t filler)
{
	filler(buffer, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
	filler(buffer, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
	for (auto it = this->child_list.begin(); it != this->child_list.end(); it++)
		filler(buffer, it->first.c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
}

uint64_t dentry::get_child_num() {
	return this->child_num;
}
uint64_t dentry::get_total_name_legth() {
	return this->total_name_length;
}
