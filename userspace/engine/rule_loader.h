/*
Copyright (C) 2022 The Falco Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>
#include "falco_source.h"
#include "falco_load_result.h"
#include "indexed_vector.h"

namespace rule_loader
{
	class context
	{
	public:
		// The kinds of items that can be in rules
		// content. These generally map to yaml items but a
		// few are more specific (e.g. "within condition
		// expression", "value for yaml node", etc.)
		enum item_type {
			VALUE_FOR = 0,
			EXCEPTIONS,
			EXCEPTION,
			EXCEPTION_VALUES,
			EXCEPTION_VALUE,
			RULES_CONTENT,
			RULES_CONTENT_ITEM,
			REQUIRED_ENGINE_VERSION,
			REQUIRED_PLUGIN_VERSIONS,
			REQUIRED_PLUGIN_VERSIONS_ENTRY,
			REQUIRED_PLUGIN_VERSIONS_ALTERNATIVE,
			LIST,
			LIST_ITEM,
			MACRO,
			MACRO_CONDITION,
			RULE,
			RULE_CONDITION,
			CONDITION_EXPRESSION,
			RULE_OUTPUT,
			RULE_OUTPUT_EXPRESSION,
			RULE_PRIORITY
		};

		static const std::string& item_type_as_string(enum item_type it);

		static const size_t default_snippet_width = 160;

		struct position
		{
			position() : pos(0), line(0), column(0) {};
			position(const YAML::Mark& mark) : pos(mark.pos), line(mark.line), column(mark.column) {};
			~position() = default;
			int pos;
			int line;
			int column;
		};

		struct location
		{
			// A name for the content this location refers
			// to. Will generally be a filename, can also
			// refer to a rule/macro condition when the
			// location points into a condition string.
			std::string name;

			// The original location in the document
			position pos;

			// The kind of item at this location
			// (e.g. "list", "macro", "rule", "exception", etc)
			context::item_type item_type;

			// The name of this item (e.g. "Write Below Etc",
			// etc).
			std::string item_name;
		};

		context(const std::string& name);
		context(const YAML::Node& item,
			item_type item_type,
			const std::string item_name,
			const context& parent);

		// Build a context from a condition expression +
		// parser position. This does not use the original
		// yaml content because:
		//   - YAML block indicators will remove whitespace/newlines/wrapping
		//     from the YAML node containing the condition expression.
		//   - When compiling, the condition expression has expanded
		//     macro and list references with their values.
		context(const libsinsp::filter::ast::pos_info& pos,
			const std::string& condition,
			const context& parent);

		virtual ~context() = default;

		// Return the content name (generally filename) for
		// this context
		const std::string& name() const;

		// Return a snippet of the provided rules content
		// corresponding to this context.
		// Uses the provided rules_contents to look up the original
		// rules content for a given location name.
		// (If this context has a non-empty alt_content, it
		// will be used to create the snippet, ignoring the
		// provided rules_contents).
		std::string snippet(const falco::load_result::rules_contents_t& rules_contents, size_t snippet_width = default_snippet_width) const;

		std::string as_string();
		nlohmann::json as_json();

	private:
		void init(const std::string& name,
			  const position& pos,
			  const item_type item_type,
			  const std::string item_name,
			  const context& parent);

		// A chain of locations from the current item, its
		// parent, possibly older ancestors.
		std::vector<location> m_locs;

		// If non-empty, this content will be used when
		// creating snippets. Used for contexts involving
		// condition expressions.
		std::string alt_content;
	};

	struct warning
	{
		falco::load_result::warning_code wc;
		std::string msg;
		context ctx;
	};

	struct error
	{
		falco::load_result::error_code ec;
		std::string msg;
		context ctx;
	};

	class rule_load_exception : public std::exception
	{
	public:
		rule_load_exception(falco::load_result::error_code ec, std::string msg, const context& ctx);
		virtual ~rule_load_exception();
		const char* what();

		falco::load_result::error_code ec;
		std::string msg;
		context ctx;

		std::string errstr;
	};

	/*!
		\brief Contains the result of loading rule definitions
	*/
	class result : public falco::load_result
	{
	public:
		result(const std::string &name);
		virtual ~result() = default;

		virtual bool successful() override;
		virtual bool has_warnings() override;

		virtual const std::string& as_string(bool verbose, const falco::load_result::rules_contents_t& contents) override;
		virtual const nlohmann::json& as_json(const falco::load_result::rules_contents_t& contents) override;

		void add_error(falco::load_result::error_code ec,
			       const std::string& msg,
			       const context& ctx);

		void add_warning(falco::load_result::warning_code ec,
				 const std::string& msg,
				 const context& ctx);
	protected:

		const std::string& as_summary_string();
		const std::string& as_verbose_string(const falco::load_result::rules_contents_t& contents);
		std::string name;
		bool success;

		std::vector<error> errors;
		std::vector<warning> warnings;

		std::string res_summary_string;
		std::string res_verbose_string;
		nlohmann::json res_json;
	};

	/*!
		\brief Contains the info required to load rule definitions
	*/
	struct configuration
	{
		explicit configuration(
			const std::string& cont,
			const indexed_vector<falco_source>& srcs,
			std::string name)
				: content(cont), sources(srcs), name(name)
			{
				res.reset(new result(name));
			}

		const std::string& content;
		const indexed_vector<falco_source>& sources;
		std::string name;
		std::unique_ptr<result> res;
		std::string output_extra;
		uint16_t default_ruleset_id;
		bool replace_output_container_info;
		falco_common::priority_type min_priority;
	};

	/*!
		\brief Represents infos about an engine version requirement
	*/
	struct engine_version_info
	{
		engine_version_info(context &ctx);
		~engine_version_info() = default;

		context ctx;
		uint32_t version;
	};

	/*!
		\brief Represents infos about a plugin version requirement
	*/
	struct plugin_version_info
	{
		struct requirement
		{
			requirement() = default;
			requirement(const std::string n, const std::string v):
				name(n), version(v) { }

			std::string name;
			std::string version;
		};

		typedef std::vector<requirement> requirement_alternatives;

		// This differs from the other _info structs by having
		// a default constructor. This allows it to be used
		// by falco_engine, which aliases the type.
		plugin_version_info();
		plugin_version_info(context &ctx);
		~plugin_version_info() = default;

		context ctx;
		requirement_alternatives alternatives;
	};

	/*!
		\brief Represents infos about a list
	*/
	struct list_info
	{
		list_info(context &ctx);
		~list_info() = default;

		context ctx;
		bool used;
		size_t index;
		size_t visibility;
		std::string name;
		std::vector<std::string> items;
	};

	/*!
		\brief Represents infos about a macro
	*/
	struct macro_info
	{
		macro_info(context &ctx);
		~macro_info() = default;

		context ctx;
		context cond_ctx;
		bool used;
		size_t index;
		size_t visibility;
		std::string name;
		std::string cond;
		std::shared_ptr<libsinsp::filter::ast::expr> cond_ast;
	};

	/*!
		\brief Represents infos about a single rule exception
	*/
	struct rule_exception_info
	{
		rule_exception_info(context &ctx);
		~rule_exception_info() = default;

		/*!
			\brief This is necessary due to the dynamic-typed nature of
			exceptions. Each of fields, comps, and values, can either be a
			single value or a list of values. This is a simple hack to make
			this easier to implement in C++, that is not non-dynamic-typed.
		*/
		struct entry {
			bool is_list;
			std::string item;
			std::vector<entry> items;

			inline bool is_valid() const
			{
				return (is_list && !items.empty())
					|| (!is_list && !item.empty());
			}
		};

		context ctx;
		std::string name;
		entry fields;
		entry comps;
		std::vector<entry> values;
	};

	/*!
		\brief Represents infos about a rule
	*/
	struct rule_info
	{
		rule_info(context &ctx);
		~rule_info() = default;

		context ctx;
		context cond_ctx;
		context output_ctx;
		size_t index;
		size_t visibility;
		std::string name;
		std::string cond;
		std::string source;
		std::string desc;
		std::string output;
		std::set<std::string> tags;
		std::vector<rule_exception_info> exceptions;
		falco_common::priority_type priority;
		bool enabled;
		bool warn_evttypes;
		bool skip_if_unknown_filter;
	};
};
