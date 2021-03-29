#include "indirect_table.hpp"

unique_ptr<inode> indirect_table::get_inode(const char *path) {
	global_logger.log(indexing_ops, "Called inode(" + std::string(path) + ")");


}

int indirect_table::create_table(ino_t ino){
	unique_ptr<dentry_table> dtable = std::make_unique<dentry_table>(ino);
	dentry_tables.insert(std::make_pair(ino, std::move(dtable)));
}

int indirect_table::delete_table(ino_t ino){
	std::map<ino_t, unique_ptr<dentry_table>>::iterator it;
	it = this->dentry_tables.find(ino);

	if(it == this->dentry_tables.end())
		return -1;

	this->dentry_tables.erase(it);

	return 0;
}