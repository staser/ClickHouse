#pragma once

#include <DB/Dictionaries/IDictionary.h>
#include <DB/Dictionaries/IDictionarySource.h>
#include <DB/Dictionaries/DictionaryStructure.h>
#include <DB/Common/HashTable/HashMap.h>
#include <DB/Columns/ColumnString.h>
#include <ext/range.hpp>
#include <atomic>
#include <memory>
#include <tuple>


namespace DB
{

class HashedDictionary final : public IDictionary
{
public:
	HashedDictionary(const std::string & name, const DictionaryStructure & dict_struct,
		DictionarySourcePtr source_ptr, const DictionaryLifetime dict_lifetime, bool require_nonempty);

	HashedDictionary(const HashedDictionary & other);

	std::exception_ptr getCreationException() const override { return creation_exception; }

	std::string getName() const override { return name; }

	std::string getTypeName() const override { return "Hashed"; }

	std::size_t getBytesAllocated() const override { return bytes_allocated; }

	std::size_t getQueryCount() const override { return query_count.load(std::memory_order_relaxed); }

	double getHitRate() const override { return 1.0; }

	std::size_t getElementCount() const override { return element_count; }

	double getLoadFactor() const override { return static_cast<double>(element_count) / bucket_count; }

	bool isCached() const override { return false; }

	DictionaryPtr clone() const override { return std::make_unique<HashedDictionary>(*this); }

	const IDictionarySource * getSource() const override { return source_ptr.get(); }

	const DictionaryLifetime & getLifetime() const override { return dict_lifetime; }

	const DictionaryStructure & getStructure() const override { return dict_struct; }

	std::chrono::time_point<std::chrono::system_clock> getCreationTime() const override
	{
		return creation_time;
	}

	bool isInjective(const std::string & attribute_name) const override
	{
		return dict_struct.attributes[&getAttribute(attribute_name) - attributes.data()].injective;
	}

	bool hasHierarchy() const override { return hierarchical_attribute; }

	void toParent(const PaddedPODArray<id_t> & ids, PaddedPODArray<id_t> & out) const override;

#define DECLARE(TYPE)\
	void get##TYPE(const std::string & attribute_name, const PaddedPODArray<id_t> & ids, PaddedPODArray<TYPE> & out) const;
	DECLARE(UInt8)
	DECLARE(UInt16)
	DECLARE(UInt32)
	DECLARE(UInt64)
	DECLARE(Int8)
	DECLARE(Int16)
	DECLARE(Int32)
	DECLARE(Int64)
	DECLARE(Float32)
	DECLARE(Float64)
#undef DECLARE

	void getString(const std::string & attribute_name, const PaddedPODArray<id_t> & ids, ColumnString * out) const;

#define DECLARE(TYPE)\
	void get##TYPE(\
		const std::string & attribute_name, const PaddedPODArray<id_t> & ids, const PaddedPODArray<TYPE> & def,\
		PaddedPODArray<TYPE> & out) const;
	DECLARE(UInt8)
	DECLARE(UInt16)
	DECLARE(UInt32)
	DECLARE(UInt64)
	DECLARE(Int8)
	DECLARE(Int16)
	DECLARE(Int32)
	DECLARE(Int64)
	DECLARE(Float32)
	DECLARE(Float64)
#undef DECLARE

	void getString(
		const std::string & attribute_name, const PaddedPODArray<id_t> & ids, const ColumnString * const def,
		ColumnString * const out) const;

#define DECLARE(TYPE)\
	void get##TYPE(\
		const std::string & attribute_name, const PaddedPODArray<id_t> & ids, const TYPE & def, PaddedPODArray<TYPE> & out) const;
	DECLARE(UInt8)
	DECLARE(UInt16)
	DECLARE(UInt32)
	DECLARE(UInt64)
	DECLARE(Int8)
	DECLARE(Int16)
	DECLARE(Int32)
	DECLARE(Int64)
	DECLARE(Float32)
	DECLARE(Float64)
#undef DECLARE

	void getString(
		const std::string & attribute_name, const PaddedPODArray<id_t> & ids, const String & def,
		ColumnString * const out) const;

	void has(const PaddedPODArray<id_t> & ids, PaddedPODArray<UInt8> & out) const override;

private:
	template <typename Value> using CollectionType = HashMap<UInt64, Value>;
	template <typename Value> using CollectionPtrType = std::unique_ptr<CollectionType<Value>>;

	struct attribute_t final
	{
		AttributeUnderlyingType type;
		std::tuple<
			UInt8, UInt16, UInt32, UInt64,
			Int8, Int16, Int32, Int64,
			Float32, Float64,
			String> null_values;
		std::tuple<
			CollectionPtrType<UInt8>, CollectionPtrType<UInt16>, CollectionPtrType<UInt32>, CollectionPtrType<UInt64>,
			CollectionPtrType<Int8>, CollectionPtrType<Int16>, CollectionPtrType<Int32>, CollectionPtrType<Int64>,
			CollectionPtrType<Float32>, CollectionPtrType<Float64>,
			CollectionPtrType<StringRef>> maps;
		std::unique_ptr<Arena> string_arena;
	};

	void createAttributes();

	void loadData();

	template <typename T>
	void addAttributeSize(const attribute_t & attribute);

	void calculateBytesAllocated();

	template <typename T>
	void createAttributeImpl(attribute_t & attribute, const Field & null_value);

	attribute_t createAttributeWithType(const AttributeUnderlyingType type, const Field & null_value);

	template <typename OutputType, typename ValueSetter, typename DefaultGetter>
	void getItemsNumber(
		const attribute_t & attribute,
		const PaddedPODArray<id_t> & ids,
		ValueSetter && set_value,
		DefaultGetter && get_default) const;

	template <typename AttributeType, typename OutputType, typename ValueSetter, typename DefaultGetter>
	void getItemsImpl(
		const attribute_t & attribute,
		const PaddedPODArray<id_t> & ids,
		ValueSetter && set_value,
		DefaultGetter && get_default) const;

	template <typename T>
	void setAttributeValueImpl(attribute_t & attribute, const id_t id, const T value);

	void setAttributeValue(attribute_t & attribute, const id_t id, const Field & value);

	const attribute_t & getAttribute(const std::string & attribute_name) const;

	template <typename T>
	void has(const attribute_t & attribute, const PaddedPODArray<id_t> & ids, PaddedPODArray<UInt8> & out) const;

	const std::string name;
	const DictionaryStructure dict_struct;
	const DictionarySourcePtr source_ptr;
	const DictionaryLifetime dict_lifetime;
	const bool require_nonempty;

	std::map<std::string, std::size_t> attribute_index_by_name;
	std::vector<attribute_t> attributes;
	const attribute_t * hierarchical_attribute = nullptr;

	std::size_t bytes_allocated = 0;
	std::size_t element_count = 0;
	std::size_t bucket_count = 0;
	mutable std::atomic<std::size_t> query_count{0};

	std::chrono::time_point<std::chrono::system_clock> creation_time;

	std::exception_ptr creation_exception;
};

}
