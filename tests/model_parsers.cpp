#include "NODParser.h"
#include "NMBParser.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

using Bytes = std::vector<uint8_t>;

static void put32(Bytes &data, size_t offset, uint32_t value) {
	for(size_t i = 0; i < 4; ++i) data[offset + i] = uint8_t(value >> (i * 8));
}

static void append32(Bytes &data, uint32_t value) {
	const size_t offset = data.size();
	data.resize(offset + 4);
	put32(data, offset, value);
}

static Bytes nod() {
	Bytes data(92 + 32 + 96 + 6 + 56 + 4, 0);
	data[0] = 10;
	data[4] = data[6] = data[7] = 1;
	put32(data, 0x28, 3);
	put32(data, 0x44, 3);
	memcpy(data.data() + 92, "test_material", 13);
	data[92 + 32 + 96 + 2] = 1;
	data[92 + 32 + 96 + 4] = 2;
	const size_t group = 92 + 32 + 96 + 6;
	data[group + 6] = 3;
	data[group + 8] = 3;
	data[group + 28] = 3;
	return data;
}

static void bundleEntry(Bytes &data, bool versionTwo = false, uint8_t vertexType = 0) {
	const size_t start = data.size();
	data.resize(start + 66, 0);
	data[start] = data[start + 2] = data[start + 37] = 1;
	memcpy(data.data() + start + 5, "test_model", 10);
	data.push_back(vertexType);
	append32(data, 3);
	const size_t strides[] = {32, 36, 48, 68, 72, 84};
	data.resize(data.size() + 3 * strides[vertexType], 0);
	append32(data, 3);
	data.insert(data.end(), {0, 0, 1, 0, 2, 0});
	const size_t shader = data.size();
	data.resize(shader + 32, 0);
	memcpy(data.data() + shader, "test_material", 13);
	append32(data, 0);
	const size_t group = data.size();
	data.resize(group + 33, 0);
	data[group + 7] = data[group + 9] = 3;
	if(versionTwo) data.push_back(0);
}

static Bytes multiSetBundle() {
	Bytes data(12 + 66, 0);
	put32(data, 0, 1);
	put32(data, 4, 1);
	data[12] = 1;
	data[14] = data[49] = 2;
	for(uint8_t type : {uint8_t(0), uint8_t(1)}) {
		data.push_back(type);
		append32(data, type ? 4 : 3);
		data.resize(data.size() + (type ? 4 * 36 : 3 * 32), 0);
	}
	append32(data, 6);
	data.insert(data.end(), {0, 0, 1, 0, 2, 0, 1, 0, 2, 0, 3, 0});
	data.resize(data.size() + 32, 0);
	append32(data, 0);
	for(uint8_t set : {uint8_t(0), uint8_t(1)}) {
		const size_t group = data.size();
		data.resize(group + 33, 0);
		data[group + 4] = set;
		data[group + 7] = 3;
		data[group + 9] = set ? 4 : 3;
	}
	return data;
}

int main(int argc, char **argv) {
	if(argc == 2) {
		char extension[16];
		FileUtils::getFileExtension(argv[1], extension, sizeof(extension));
		if(!strcmp(extension, "nod")) {
			NODParser parser;
			if(parser.parse(argv[1])) return 0;
			std::cerr << parser.getLastError() << '\n';
		} else if(!strcmp(extension, "nmb")) {
			auto parser = std::make_unique<NMBParser>();
			if(parser->parse(argv[1])) return 0;
			std::cerr << parser->getLastError() << '\n';
		} else {
			std::cerr << "Expected NOD or NMB input\n";
		}
		return 1;
	}
	NODParser model;
	Bytes good = nod();
	assert(model.parseBuffer(good.data(), good.size()));
	assert(model.getModelData().index_count == 3);
	assert(model.getModelData().mesh_groups[0].lods[0].list_count == 3);
	assert(model.parseBuffer(good.data(), good.size()));
	for(size_t size : {size_t(0), size_t(91), size_t(100), good.size() - 5}) {
		assert(!model.parseBuffer(good.data(), size));
		assert(!model.getLastError().empty());
		assert(model.getModelData().vertex_data == nullptr);
	}
	Bytes bad = good;
	bad[6] = 5;
	assert(!model.parseBuffer(bad.data(), bad.size()));
	bad = good;
	bad[4] = 65;
	assert(!model.parseBuffer(bad.data(), bad.size()));
	bad = good;
	put32(bad, 0x24, 6);
	assert(!model.parseBuffer(bad.data(), bad.size()));
	bad = good;
	bad[92 + 32 + 96 + 6 + 54] = 1;
	assert(!model.parseBuffer(bad.data(), bad.size()));
	assert(model.parseBuffer(good.data(), good.size()));

	NMBParser bundle;
	Bytes packed(12, 0);
	put32(packed, 0, 1);
	put32(packed, 4, 2);
	bundleEntry(packed);
	bundleEntry(packed);
	assert(bundle.parseBuffer(packed.data(), packed.size()));
	assert(bundle.getBundleData().parsed_count == 2);
	assert(bundle.getBundleData().models[0].vertex_sets[0].base_stride == 32);
	for(int i = 0; i < 100; ++i) assert(bundle.parseBuffer(packed.data(), packed.size()));
	assert(!bundle.parseBuffer(packed.data(), packed.size() - 10));
	assert(!bundle.getLastError().empty());
	assert(bundle.getBundleData().parsed_count == 0);
	assert(bundle.getBundleData().models[0].vertex_data == nullptr);
	put32(packed, 4, 257);
	assert(!bundle.parseBuffer(packed.data(), packed.size()));
	put32(packed, 4, 2);
	assert(bundle.parseBuffer(packed.data(), packed.size()));
	packed.assign(12, 0);
	put32(packed, 0, 2);
	put32(packed, 4, 2);
	bundleEntry(packed, true, 3);
	packed.back() = 1;
	packed.insert(packed.end(), 12, 255);
	bundleEntry(packed, true);
	assert(bundle.parseBuffer(packed.data(), packed.size()));
	assert(bundle.getBundleData().models[0].color_count == 3);
	assert(bundle.getBundleData().models[0].vertex_colors[3] == 255);
	assert(bundle.getBundleData().models[0].vertex_sets[0].stride == 68);
	assert(bundle.getBundleData().models[0].vertex_sets[0].base_stride == 32);
	assert(bundle.getBundleData().models[1].color_count == 0);
	packed = multiSetBundle();
	assert(bundle.parseBuffer(packed.data(), packed.size()));
	const auto &entry = bundle.getBundleData().models[0];
	assert(entry.mesh_groups[0].lods[0].list_start == 0);
	assert(entry.mesh_groups[1].lods[0].list_start == 3);
	assert(entry.indices[entry.mesh_groups[1].lods[0].list_start] == 1);
	packed[packed.size() - 33 + 7] = 6;
	assert(!bundle.parseBuffer(packed.data(), packed.size()));
	packed = multiSetBundle();
	packed[packed.size() - 33] = 1;
	assert(!bundle.parseBuffer(packed.data(), packed.size()));
	std::cout << "NOD/NMB parser regression tests passed\n";
}
