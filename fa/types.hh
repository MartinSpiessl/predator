/*
 * Copyright (C) 2010 Jiri Simacek
 *
 * This file is part of predator.
 *
 * predator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * predator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with predator.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <ostream>
#include <cassert>
#include <stdexcept>

#include <vector>
#include <boost/unordered_map.hpp>

struct SelData {

	size_t	offset;
	int		size;
	int		displ;

	SelData(size_t offset, int size, int displ) : offset(offset), size(size), displ(displ) {}

	static SelData fromArgs(const std::vector<std::string>& args) {
		if (args.size() != 4)
			throw std::runtime_error("incorrect number of arguments");
	 	return SelData(atol(args[1].c_str()), atol(args[2].c_str()), atol(args[3].c_str()));
	}

	friend size_t hash_value(const SelData& v) {
		return boost::hash_value(v.offset + v.size + v.displ);
	}

	bool operator==(const SelData& rhs) const {
		return this->offset == rhs.offset && this->size == rhs.size && this->displ == rhs.displ;
	}

	friend std::ostream& operator<<(std::ostream& os, const SelData& x) {
		os << "sel" << x.offset << ':' << x.size << '[';
		if (x.displ >= 0)
			os << '+';
		return os << x.displ << ']';
	}

};

struct Data {

	typedef std::pair<size_t, Data> item_info;

	typedef enum {
		t_undef,
		t_unknw,
		t_native_ptr,
		t_void_ptr,
		t_ref,
		t_int,
		t_bool,
		t_struct,
		t_other
	} type_enum;

	type_enum type;

	int size;

	union {
		void*	d_native_ptr;
		size_t	d_void_ptr;
		struct {
			size_t	root;
			int		displ;			
		}		d_ref;
		int		d_int;
		bool	d_bool;
		std::vector<item_info>* d_struct;
	};

	Data(type_enum type = type_enum::t_undef) : type(type), size(0) {}

	Data(const Data& data) : type(data.type), size(data.size) {
		switch (data.type) {
			case type_enum::t_native_ptr: this->d_native_ptr = data.d_native_ptr; break;
			case type_enum::t_void_ptr: this->d_void_ptr = data.d_void_ptr; break;
			case type_enum::t_ref: this->d_ref.root = data.d_ref.root; this->d_ref.displ = data.d_ref.displ; break;
			case type_enum::t_int: this->d_int = data.d_int; break;
			case type_enum::t_bool: this->d_bool = data.d_bool; break;
			case type_enum::t_struct: this->d_struct = new std::vector<item_info>(*data.d_struct); break;
			default: break;
		}
	}

	~Data() { this->clear(); }

	Data& operator=(const Data& rhs) {
		this->clear();
		this->type = rhs.type;
		this->size = rhs.size;
		switch (rhs.type) {
			case type_enum::t_native_ptr: this->d_native_ptr = rhs.d_native_ptr; break;
			case type_enum::t_void_ptr: this->d_void_ptr = rhs.d_void_ptr; break;
			case type_enum::t_ref: this->d_ref.root = rhs.d_ref.root; this->d_ref.displ = rhs.d_ref.displ; break;
			case type_enum::t_int: this->d_int = rhs.d_int; break;
			case type_enum::t_bool: this->d_bool = rhs.d_bool; break;
			case type_enum::t_struct: this->d_struct = new std::vector<item_info>(*rhs.d_struct); break;
			default: break;
		}
		return *this;
	}

	static Data createUndef() { return Data(); }

	static Data createUnknw() { return Data(type_enum::t_unknw); }

	static Data createNativePtr(void* ptr) {
		Data data(type_enum::t_native_ptr);
		data.d_native_ptr = ptr;
		return data;
	}

	static Data createRef(size_t root, int displ = 0) {
		Data data(type_enum::t_ref);
		data.d_ref.root = root;
		data.d_ref.displ = displ;
		return data;
	}

	static Data createStruct(const std::vector<item_info>& items = std::vector<item_info>()) {
		Data data(type_enum::t_struct);
		data.d_struct = new std::vector<item_info>(items);
		return data;
	}

	static Data createVoidPtr(size_t size = 0) {
		Data data(type_enum::t_void_ptr);
		data.d_void_ptr = size;
		return data;
	}

	static Data createInt(int x) {
		Data data(type_enum::t_int);
		data.d_int = x;
		return data;
	}

	static Data createBool(bool x) {
		Data data(type_enum::t_bool);
		data.d_bool = x;
		return data;
	}

	static Data fromArgs(const std::vector<std::string>& args) {
		if (args[1] == "int") {
			if (args.size() != 3)
				throw std::runtime_error("incorrect number of arguments");
		 	Data data(type_enum::t_int);
		 	data.d_int = atol(args[2].c_str());
		 	return data;
		}
		if (args[1] == "ref") {
			if (args.size() != 4)
				throw std::runtime_error("incorrect number of arguments");
		 	Data data(type_enum::t_ref);
		 	data.d_ref.root = atol(args[2].c_str());
		 	data.d_ref.displ = atol(args[3].c_str());
		 	return data;
		}
		throw std::runtime_error("non-parsable arguments");
	}

	void clear() {
		if (this->type == type_enum::t_struct)
			delete this->d_struct;
		this->type = type_enum::t_undef;
	}

	bool isDefined() const {
		return this->type != type_enum::t_undef;
	}

	bool isUndef() const {
		return this->type == type_enum::t_undef;
	}

	bool isUnknw() const {
		return this->type == type_enum::t_unknw;
	}

	bool isNativePtr() const {
		return this->type == type_enum::t_native_ptr;
	}

	bool isVoidPtr() const {
		return this->type == type_enum::t_void_ptr;
	}

	bool isNull() const {
		return this->type == type_enum::t_void_ptr && this->d_void_ptr == 0;
	}

	bool isRef() const {
		return this->type == type_enum::t_ref;
	}

	bool isRef(size_t root) const {
		return this->type == type_enum::t_ref && this->d_ref.root == root;
	}

	bool isStruct() const {
		return this->type == type_enum::t_struct;
	}

	bool isBool() const {
		return this->type == type_enum::t_bool;
	}

	bool isInt() const {
		return this->type == type_enum::t_int;
	}

	friend size_t hash_value(const Data& v) {
		switch (v.type) {
			case type_enum::t_undef:
			case type_enum::t_unknw: return boost::hash_value(v.type);
			case type_enum::t_native_ptr: return boost::hash_value(v.type + boost::hash_value(v.d_native_ptr));
			case type_enum::t_void_ptr: return boost::hash_value(v.type + v.d_void_ptr);
			case type_enum::t_ref: return boost::hash_value(v.type + v.d_ref.root + v.d_ref.displ);
			case type_enum::t_int: return boost::hash_value(v.type + v.d_int);
			case type_enum::t_bool: return boost::hash_value(v.type + v.d_bool);
			case type_enum::t_struct: return boost::hash_value(v.type + boost::hash_value(*v.d_struct));
			default: return boost::hash_value(v.type + v.d_void_ptr);
		}
	}

	bool operator==(const Data& rhs) const {
		if (this->type != rhs.type)
			return false;
		switch (this->type) {
			case type_enum::t_undef:
			case type_enum::t_unknw: return true;
			case type_enum::t_void_ptr: return this->d_void_ptr == rhs.d_void_ptr;
			case type_enum::t_native_ptr: return this->d_native_ptr == rhs.d_native_ptr;
			case type_enum::t_ref: return this->d_ref.root == rhs.d_ref.root && this->d_ref.displ == rhs.d_ref.displ;
			case type_enum::t_int: return this->d_int == rhs.d_int;
			case type_enum::t_bool: return this->d_bool == rhs.d_bool;
			case type_enum::t_struct: return *this->d_struct == *rhs.d_struct;
			default: return false;
		}
	}

	friend std::ostream& operator<<(std::ostream& os, const Data& x) {
//		os << '[' << x.size << ']';
		switch (x.type) {
			case type_enum::t_undef: os << "(undef)"; break;
			case type_enum::t_unknw: os << "(unknw)"; break;
			case type_enum::t_native_ptr: os << "(native_ptr)" << x.d_native_ptr; break;
			case type_enum::t_void_ptr: os << "(void_ptr)" << x.d_void_ptr; break;
			case type_enum::t_ref: os << "(ref)" << x.d_ref.root << '+' << x.d_ref.displ; break;
			case type_enum::t_int: os << "(int)" << x.d_int; break;
			case type_enum::t_bool: os << "(bool)" << x.d_bool; break;
			case type_enum::t_struct:
				os << "(struct)[ ";
				for (std::vector<item_info>::iterator i = x.d_struct->begin(); i != x.d_struct->end(); ++i)
					os << '+' << i->first << ": " << i->second << ' ';
				os << "]";
				break;
			default: os << "(other)"; break;
		}
		return os;
	}

};

#endif
