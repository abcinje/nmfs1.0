#include "dentry.hpp"

extern rados_io *meta_pool;

dentry::dentry(ino_t ino) : this_ino(ino)
{
	global_logger.log("Called dentry(" + std::to_string(ino) +")");
	char *raw_data = (char *)malloc(MAX_DENTRY_OBJ_SIZE);
	try {
		meta_pool->read("d&" + std::to_string(ino), raw_data, MAX_DENTRY_OBJ_SIZE, 0);
		this->deserialize(raw_data);
	} catch(rados_io::no_such_object &e){
		throw std::runtime_error("Dentry Corrupted: inode number " + std::to_string(ino));
	}
}

void dentry::add_new_child(const std::string &filename, ino_t ino){
	global_logger.log("Called dentry.add_new_child()");
	global_logger.log("file : " + filename + " inode number : " + std::to_string(ino));
	this->child_list.insert(std::make_pair(filename, ino));
	this->child_num++;
}

void dentry::delete_child(const std::string &filename) {
	global_logger.log("Called dentry.delete_child()");
	global_logger.log("file : " + filename);
	this->child_list.erase(filename);
	this->child_num--;
}

unique_ptr<char> dentry::serialize()
{
	global_logger.log("Called dentry.serialize()");
	unique_ptr<char> raw(new char(sizeof(uint64_t) + ((this->child_num) * RAW_LINE_SIZE)));
	char *pointer = raw.get();

	memset(pointer, '\0', sizeof(uint64_t) + ((this->child_num) * RAW_LINE_SIZE));
	memcpy(pointer, &(this->child_num), sizeof(uint64_t));
	pointer += sizeof(uint64_t);

	for(auto it = this->child_list.begin(); it != this->child_list.end(); it++){
		/* serialize name */
		memcpy(pointer, &(it->first), it->first.length());
		/*serialize ino */
		memcpy(pointer + 256, &(it->second), sizeof(ino_t));

		pointer += RAW_LINE_SIZE;
	}

	return std::move(raw);
}

void dentry::deserialize(char *raw)
{
	global_logger.log("Called dentry.deserialize()");
	char *pointer = raw;

	memcpy(&(this->child_num), pointer, sizeof(uint64_t));
	pointer = pointer + sizeof(uint64_t);

	global_logger.log("dentry child num : " + std::to_string(this->child_num));

	for(int i = 0; i < this->child_num; i++){
		std::string name;
		memcpy(name.data(), pointer, 256);
		ino_t ino;
		memcpy(&ino, pointer + 256, sizeof(ino_t));

		this->child_list.insert(std::pair<std::string, ino_t>(name, ino));
		pointer += RAW_LINE_SIZE;

		global_logger.log("child name : " + name + " child ino : " + std::to_string(ino));
	}

}

void dentry::sync()
{global_logger.log("Called dentry.sync()");
	unique_ptr<char> raw = this->serialize();
	meta_pool->write("d$" + std::to_string(this->this_ino), raw.get(), sizeof(uint64_t) + ((this->child_num) * RAW_LINE_SIZE), 0);
}

ino_t dentry::get_child_ino(std::string child_name)
{
	global_logger.log("Called dentry.get_child_ino(" + child_name + ")");
	std::map<std::string, ino_t>::iterator ret = child_list.find(child_name);
	if(ret == child_list.end())
		return -1;
	else
		return ret->second;
}

void dentry::fill_filler(void *buffer, fuse_fill_dir_t filler)
{
	for (auto it = this->child_list.begin(); it != this->child_list.end(); it++)
		filler(buffer, it->first.c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
}

uint64_t dentry::get_child_num() {return this->child_num;}