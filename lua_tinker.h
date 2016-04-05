// lua_tinker.h
//
// LuaTinker - Simple and light C++ wrapper for Lua.
//
// Copyright (c) 2005-2007 Kwon-il Lee (zupet@hitel.net)
// 
// please check Licence.txt file for licence and legal issues. 

#if !defined(_LUA_TINKER_H_)
#define _LUA_TINKER_H_

#include <new>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <typeinfo>
#include <type_traits>
#include"lua.hpp"
#include"type_traits_ext.h" 
#include<memory>
#include<typeindex>
#include<functional>


#ifdef  _DEBUG
#define USE_TYPEID_OF_USERDATA
#endif //  _DEBUG

//#define _ALLOW_SHAREDPTR_INVOKE

#ifdef LUA_CALL_CFUNC_NEED_ALL_PARAM
#define LUA_CHECK_HAVE_THIS_PARAM(L,index) if(lua_isnone(L,index)){lua_pushfstring(L, "need argument %d to call cfunc", index);lua_error(L);}
#define LUA_CHECK_HAVE_THIS_PARAM_AND_NOT_NIL(L,index) if(lua_isnoneornil(L,index)){lua_pushfstring(L, "need argument %d to call cfunc", index);lua_error(L);}
#else
#define LUA_CHECK_HAVE_THIS_PARAM(L,index)
#define LUA_CHECK_HAVE_THIS_PARAM_AND_NOT_NIL(L,index)
#endif

#define CHECK_CLASS_PTR(T) {if(lua_isnoneornil(L,1)){lua_pushfstring(L, "class_ptr %s is nil or none", lua_tinker::get_class_name<T>());lua_error(L);} }

#define TRY_LUA_TINKER_INVOKE() try
#define CATCH_LUA_TINKER_INVOKE() catch(...)


namespace lua_tinker
{


	static const char* S_SHARED_PTR_NAME = "__shared_ptr";
	// init LuaTinker
	void    init(lua_State *L);

	void	init_shared_ptr(lua_State *L);

	// string-buffer excution
	void    dofile(lua_State *L, const char *filename);
	void    dostring(lua_State *L, const char* buff);
	void    dobuffer(lua_State *L, const char* buff, size_t sz);

	// debug helpers
	void    enum_stack(lua_State *L);
	int     on_error(lua_State *L);
	void    print_error(lua_State *L, const char* fmt, ...);

	// class helper
	int meta_get(lua_State *L);
	int meta_set(lua_State *L);
	void push_meta(lua_State *L, const char* name);

	template<typename T>
	struct class_name
	{
		// global name
		static const char* name(const char* name = NULL)
		{
			return name_str(name).c_str();
		}
		static const std::string& name_str(const char* name = NULL)
		{
			static std::string s_name;
			if (name != NULL) s_name.assign(name);
			return s_name;
		}
	};

	template<typename T>
	using base_type = typename std::remove_cv<typename std::remove_reference<typename std::remove_pointer<T>::type>::type>::type;

	template<typename T>
	static constexpr typename std::enable_if<!is_shared_ptr<T>::value, const char*>::type get_class_name()
	{
		return class_name< base_type<T> >::name();
	}

	template<typename T>
	static typename std::enable_if<is_shared_ptr<T>::value, const char*>::type get_class_name()
	{
		const std::string& strSharedName = class_name<T>::name_str();
		if (strSharedName.empty())
		{
			return S_SHARED_PTR_NAME;
		}
		else
		{
			return strSharedName.c_str();
		}


	}

	template<typename T>
	constexpr const size_t get_type_idx()
	{
		return typeid(base_type<T>).hash_code();
	}

	// dynamic type extention
	struct lua_value
	{
		virtual ~lua_value() {}
		virtual void to_lua(lua_State *L) = 0;
	};

	// type trait
	template<typename T> struct class_name;
	struct table;

	//delcare
	template<typename T>
	decltype(auto) read(lua_State *L, int index);
	template<typename T>
	decltype(auto) read_nocheck(lua_State *L, int index);
	template<typename T>
	void push(lua_State *L, T ret);	//here need a T/T*/T& not a T&&




	// from lua
	// param to pointer
	template<typename T>
	typename std::enable_if<std::is_pointer<T>::value, T>::type void2type(void* ptr)
	{
		return (base_type<T>*)ptr;
	}
	//to reference
	template<typename T>
	typename std::enable_if<std::is_reference<T>::value, base_type<T>&>::type void2type(void* ptr)
	{
		return *(base_type<T>*)ptr;
	}

	//to val
	template<typename T>
	typename std::enable_if<!is_shared_ptr<T>::value && !std::is_pointer<T>::value && !std::is_reference<T>::value, base_type<T>>::type void2type(void* ptr)
	{
		return *(base_type<T>*)ptr;
	}

	//to shared_ptr, use weak_ptr to hold it
	template<typename T>
	typename std::enable_if<is_shared_ptr<T>::value, T>::type void2type(void* ptr)
	{
		return ((std::weak_ptr<get_shared_t<T>>*)ptr)->lock();
	}

	//userdata to T，T*，T&
	template<typename T>
	T user2type(lua_State *L, int index)
	{
		return void2type<T>(lua_touserdata(L, index));
	}





	//userdata holder
	struct UserDataWapper
	{
		template<typename T>
		explicit UserDataWapper(T* p)
			: m_p(p)
#ifdef USE_TYPEID_OF_USERDATA
			, m_type_idx(get_type_idx<T>())
#endif
		{}

#ifdef USE_TYPEID_OF_USERDATA
		template<typename T>
		explicit UserDataWapper(T* p, size_t nTypeIdx)
			: m_p(p)
			, m_type_idx(nTypeIdx)
		{}
#endif

		virtual ~UserDataWapper() {}

		void* m_p;
#ifdef USE_TYPEID_OF_USERDATA
		size_t  m_type_idx;
#endif
	};

	template <class... Args>
	struct class_tag
	{
	};

	template<typename T>
	struct val2user : UserDataWapper
	{
		val2user() : UserDataWapper(new T) { }
		val2user(const T& t) : UserDataWapper(new T(t)) {}
		val2user(T&& t) : UserDataWapper(new T(std::forward<T>(t))) {}

		//tuple is hold the params, so unpack it
		//template<typename Tup, size_t ...index>
		//val2user(Tup&& tup, std::index_sequence<index...>) : UserDataWapper(new T(std::get<index>(std::forward<Tup>(tup))...)) {}

		//template<typename Tup,typename = typename std::enable_if<is_tuple<Tup>::value, void>::type >
		//val2user(Tup&& tup) : val2user(std::forward<Tup>(tup), std::make_index_sequence<std::tuple_size<typename std::decay<Tup>::type>::value>{}) {}


		//direct read args, use type_list to help hold Args
		template<typename ...Args, size_t ...index>
		val2user(lua_State* L, std::index_sequence<index...>, class_tag<Args...> tag)
			: UserDataWapper(new T(read<Args>(L, 2 + index)...))
		{}

		template<typename ...Args>
		val2user(lua_State* L, class_tag<Args...> tag)
			: val2user(L, std::make_index_sequence<sizeof...(Args)>(), tag)
		{}


		~val2user() { delete ((T*)m_p); }

	};

	template<typename T>
	struct ptr2user : UserDataWapper
	{
		ptr2user(T* t) : UserDataWapper(t) {}

	};

	template<typename T>
	struct ref2user : UserDataWapper
	{
		ref2user(T& t) : UserDataWapper(&t) {}

	};

	template<typename T>
	struct sharedptr2user : UserDataWapper
	{
		sharedptr2user(const std::shared_ptr<T>& rht)
			:UserDataWapper(&m_holder
#ifdef USE_TYPEID_OF_USERDATA
			, get_type_idx<std::shared_ptr<T>>()		
#endif
				)
			, m_holder(rht)
		{}

		//use weak_ptr to hold it
		~sharedptr2user() { m_holder.reset(); }

		std::weak_ptr<T> m_holder;
	};

	// to lua
	// userdata pointer to lua 
	template<typename T>
	typename std::enable_if<std::is_pointer<T>::value, void>::type object2lua(lua_State *L, T&& input)
	{
		if (input) new(lua_newuserdata(L, sizeof(ptr2user<base_type<T>>))) ptr2user<base_type<T>>(std::forward<T>(input)); else lua_pushnil(L);
	}
	// userdata reference to lua
	template<typename T>
	typename std::enable_if<std::is_reference<T>::value, void>::type object2lua(lua_State *L, T&& input)
	{
		new(lua_newuserdata(L, sizeof(ref2user<T>))) ref2user<T>(std::forward<T>(input));
	}
	// userdata val to lua
	template<typename T>
	typename std::enable_if<!std::is_pointer<T>::value && !std::is_reference<T>::value, void>::type object2lua(lua_State *L, T&& input)
	{
		new(lua_newuserdata(L, sizeof(val2user<T>))) val2user<T>(std::forward<T>(input));
	}

	// shared_ptr to lua 
	template<typename T>
	void sharedobject2lua(lua_State *L, std::shared_ptr<T> input)
	{
		if (input) new(lua_newuserdata(L, sizeof(sharedptr2user<T>))) sharedptr2user<T>(input); else lua_pushnil(L);
	}



	// get value from cclosure
	template<typename T>
	T upvalue_(lua_State *L)
	{
		return user2type<T>(L, lua_upvalueindex(1));
	}







	// lua stack help to read/push
	template<typename T, typename Enable = void>
	struct _stack_help
	{
		static T _read(lua_State *L, int index)
		{
			return lua2type<T>(L, index);
		}

		//get userdata ptr from lua, can handle nil an 0
		template<typename _T>
		static typename std::enable_if<std::is_pointer<_T>::value, _T>::type lua2type(lua_State *L, int index)
		{
			if (lua_isnoneornil(L, index))
			{
				return nullptr;
			}
			else if (lua_isnumber(L, index) && lua_tonumber(L, index) == 0)
			{
				return nullptr;
			}
			return _lua2type<_T>(L, index);
		}

		//get userdata from lua 
		template<typename _T>
		static typename std::enable_if<!std::is_pointer<_T>::value, _T>::type lua2type(lua_State *L, int index)
		{
			return _lua2type<_T>(L, index);
		}

		template<typename _T>
		static _T _lua2type(lua_State *L, int index)
		{
			if (!lua_isuserdata(L, index))
			{
				lua_pushfstring(L, "can't convert argument %d to class %s", index, get_class_name<_T>());
				lua_error(L);
			}


			UserDataWapper* pWapper = user2type<UserDataWapper*>(L, index);
#ifdef USE_TYPEID_OF_USERDATA
			if (pWapper->m_type_idx != get_type_idx<base_type<_T>>())
			{
				lua_pushfstring(L, "can't convert argument %d to class %s", index, get_class_name<T>());
				lua_error(L);
			}
#endif

			return void2type<T>(pWapper->m_p);
		}


		//obj to lua
		template<typename _T>
		static typename std::enable_if<!is_shared_ptr<_T>::value, void>::type _push(lua_State *L, _T&& val)
		{
			lua_tinker::object2lua(L, std::forward<_T>(val));
			push_meta(L, get_class_name<_T>());
			lua_setmetatable(L, -2);
		}

		//shared_ptr to lua
		template<typename _T>
		static typename std::enable_if<is_shared_ptr<_T>::value, void>::type _push(lua_State *L, _T&& val)
		{
			lua_tinker::sharedobject2lua(L, std::forward<_T>(val));
			push_meta(L, get_class_name<_T>());
			lua_setmetatable(L, -2);
		}
	};

	template<>
	struct _stack_help<char*>
	{
		static char* _read(lua_State *L, int index);
		static void  _push(lua_State *L, char* ret);
	};

	template<>
	struct _stack_help<const char*>
	{
		static const char* _read(lua_State *L, int index);
		static void  _push(lua_State *L, const char* ret);
	};

	template<>
	struct _stack_help<bool>
	{
		static bool _read(lua_State *L, int index);
		static void  _push(lua_State *L, bool ret);
	};

	//integral
	template<typename T>
	struct _stack_help<T, typename std::enable_if<std::is_integral<T>::value>::type>
	{
		static T _read(lua_State *L, int index)
		{
			return (T)lua_tointeger(L, index);
		}
		static void  _push(lua_State *L, T ret)
		{
			lua_pushinteger(L, ret);
		}
	};

	//float pointer
	template<typename T>
	struct _stack_help<T, typename std::enable_if<std::is_floating_point<T>::value>::type>
	{
		static T _read(lua_State *L, int index)
		{
			return (T)lua_tonumber(L, index);
		}
		static void  _push(lua_State *L, T ret)
		{
			lua_pushnumber(L, ret);
		}
	};

	template<>
	struct _stack_help<std::string>
	{
		static std::string _read(lua_State *L, int index);
		static void _push(lua_State *L, const std::string& ret);
	};
	template<>
	struct _stack_help<const std::string&>
	{
		static std::string _read(lua_State *L, int index);
		static void _push(lua_State *L, const std::string& ret);


	};


	template<>
	struct _stack_help<table>
	{
		static table _read(lua_State *L, int index);
		static void _push(lua_State *L,const table& ret);
	};

	template<>
	struct _stack_help<lua_value*>
	{
		static lua_value* _read(lua_State *L, int index);
		static void _push(lua_State *L, lua_value* ret);
	};

	//enum
	template<typename T>
	struct _stack_help<T, typename std::enable_if<std::is_enum<T>::value>::type>
	{
		static T _read(lua_State *L, int index)
		{
			return (T)lua_tointeger(L, index);
		}
		static void  _push(lua_State *L, T ret)
		{
			lua_pushinteger(L, (int)ret);
		}
	};

	//stl container
	template<typename T>
	struct _stack_help<T, typename std::enable_if<is_container<base_type<T>>::value>::type>
	{
		static T _read(lua_State *L, int index)
		{
			return _readfromtable<T>(L, index);
		}

		//support map,multimap,unordered_map,unordered_multimap
		template<typename _T>
		static typename std::enable_if<is_associative_container<_T>::value, _T>::type _readfromtable(lua_State *L, int index)
		{
			if (!lua_istable(L, index))
			{
				lua_pushfstring(L, "convert k-v container from argument %d must be a table", index);
				lua_error(L);
			}

			_T t;
			lua_pushnil(L);
			while (lua_next(L, -2) != 0)
			{
				if (lua_isnoneornil(L, -2) || lua_isnoneornil(L, -1))
					break;
				t.emplace(std::make_pair(read<T::key_type>(L, -2), read<T::mapped_type>(L, -1)));
				lua_remove(L, -1);
			}
			return t;
		}

		//support list,vector,deque
		template<typename _T>
		static typename std::enable_if<!is_associative_container<_T>::value, _T>::type _readfromtable(lua_State *L, int index)
		{
			if (!lua_istable(L, index))
			{
				lua_pushfstring(L, "convert container from argument %d must be a table", index);
				lua_error(L);
			}

			_T t;
			lua_pushnil(L);
			while (lua_next(L, -2) != 0)
			{
				if (lua_isnoneornil(L, -2) || lua_isnoneornil(L, -1))
					break;
				t.emplace_back(read<T::value_type>(L, -1));
				lua_remove(L, -1);
			}
			return t;
		}



		//k,v container to lua
		template<typename _T>
		static typename std::enable_if<is_associative_container<_T>::value, void>::type  _push(lua_State *L, const _T& ret)
		{
			lua_newtable(L);
			for (auto it = ret.begin(); it != ret.end(); it++)
			{
				push(L, it->first);
				push(L, it->second);
				lua_settable(L, -3);
			}
		}
		//t container to lua
		template<typename _T>
		static typename std::enable_if<!is_associative_container<_T>::value, void>::type  _push(lua_State *L, const _T& ret)
		{
			lua_newtable(L);
			auto it = ret.begin();
			for (int i = 1; it != ret.end(); it++, i++)
			{
				push(L, i);
				push(L, *it);
				lua_settable(L, -3);
			}
		}
	};

	//read_weap
	template<typename T>
	decltype(auto) read(lua_State *L, int index)
	{
#ifdef LUA_CALL_CFUNC_NEED_ALL_PARAM
		if (std::is_pointer<T>)
		{
			LUA_CHECK_HAVE_THIS_PARAM(L, index);
		}
		else
		{
			LUA_CHECK_HAVE_THIS_PARAM_AND_NOT_NIL(L, index);
		}
#endif
		return _stack_help<T>::_read(L, index);
	}

	//read_weap
	template<typename T>
	decltype(auto) read_nocheck(lua_State *L, int index)
	{
		return _stack_help<T>::_read(L, index);
	}

	//push warp
	template<typename T>
	void push(lua_State *L, T ret)	//here need a T/T*/T& not a T&&
	{
		_stack_help<T>::_push(L, std::forward<T>(ret));
	}

	// push value_list to lua stack //here need a T/T*/T& not a T&&
	template<typename T, typename ...Args>
	void push_args(lua_State *L, T ret, Args...args) { push<T>(L, std::forward<T>(ret)); push_args<Args...>(L, std::forward<Args>(args)...); }
	template<typename T, typename ...Args>
	void push_args(lua_State *L, T ret) { push<T>(L, std::forward<T>(ret)); }


	// pop a value from lua stack
	template<typename T>
	T pop(lua_State *L) { T t = read_nocheck<T>(L, -1); lua_pop(L, 1); return t; }

	template<>  void    pop(lua_State *L);
	template<>  table   pop(lua_State *L);


	//invoke func tuple hold params
	//template<typename Func, typename Tup, std::size_t... index>
	//decltype(auto) invoke_helper(Func&& func, Tup&& tup, std::index_sequence<index...>)
	//{
	//	return std::invoke(func, std::get<index>(std::forward<Tup>(tup))...);
	//}

	//template<typename Func, typename Tup>
	//decltype(auto) invoke_func(Func&& func, Tup&& tup)
	//{
	//	constexpr auto Size = std::tuple_size<typename std::decay<Tup>::type>::value;
	//	return invoke_helper(std::forward<Func>(func),
	//		std::forward<Tup>(tup),
	//		std::make_index_sequence<Size>{});
	//}

	template<typename RVal, typename Func>
	RVal invoke_func(Func func)
	{
		return func();
	}

	//direct invoke func
	template<int nIdxParams, typename RVal, typename Func, typename ...Args, std::size_t... index>
	RVal direct_invoke_invoke_helper(Func&& func, lua_State *L, std::index_sequence<index...>)
	{
		return stdext::invoke(std::forward<Func>(func), read<Args>(L, index + nIdxParams)...);
	}

	template<int nIdxParams, typename RVal, typename Func, typename ...Args>
	RVal direct_invoke_func(Func&& func, lua_State *L)
	{
		return direct_invoke_invoke_helper<nIdxParams, RVal, Func, Args...>(std::forward<Func>(func), L, std::make_index_sequence<sizeof...(Args)>{});
	}

	//make params to tuple
	//template<typename...T>
	//struct ParamHolder
	//{
	//	typedef std::tuple<typename std::remove_cv<typename std::remove_reference<T>::type>::type...> type;
	//};
	//template<typename...T>
	//using ParamHolder_T = typename ParamHolder<T...>::type;


	//template <int nIdxParams, typename... T, std::size_t... N>
	//ParamHolder_T<T...> _get_args(lua_State *L, std::index_sequence<N...>)
	//{
	//	return std::forward<ParamHolder_T<T...>>(ParamHolder_T<T...>{ read<T>(L, N + nIdxParams)... });
	//}

	//template <int nIdxParams, typename... T>
	//ParamHolder_T<T...> _get_args(lua_State *L)
	//{
	//	constexpr std::size_t num_args = sizeof...(T);
	//	return _get_args<nIdxParams, T...>(L, std::make_index_sequence<num_args>());
	//}

	static bool CheckSameMetaTable(lua_State* L, int nIndex, const char* tname)
	{
		bool bResult = true;
		void *p = lua_touserdata(L, nIndex);
		if (p != NULL)
		{  /* value is a userdata? */
			if (lua_getmetatable(L, nIndex))
			{  /* does it have a metatable? */
				push_meta(L, tname);  /* get correct metatable */
				if (!lua_rawequal(L, -1, -2))  /* not the same? */
					bResult = false;  /* value is a userdata with wrong metatable */
				lua_pop(L, 2);  /* remove both metatables */
				return bResult;
			}
		}
		return false;
	}

	//functor
	template <typename CT, typename RVal, typename ... Args>
	struct member_functor
	{
		typedef std::function< RVal(CT*, Args...) > FunctionType;
		FunctionType m_func;

		member_functor(FunctionType func)
			:m_func(func)
		{}

		~member_functor()
		{}

		static int invoke(lua_State *L)
		{
			CHECK_CLASS_PTR(CT);
			TRY_LUA_TINKER_INVOKE()
			{
				_invoke<RVal>(L);
				return 1;
			}
			CATCH_LUA_TINKER_INVOKE()
			{
				lua_pushfstring(L, "lua fail to invoke functor");
				lua_error(L);
			}
			return 0;
		}

		template<typename T>
		static typename std::enable_if<!std::is_void<T>::value, void>::type _invoke(lua_State *L)
		{
			using FuncType = RVal(CT::*)(Args...);
#ifdef _ALLOW_SHAREDPTR_INVOKE
			if(CheckSameMetaTable(L,1,get_class_name<CT>()) == false)
				push<RVal>(L, direct_invoke_func<1, RVal, FuncType, std::shared_ptr<CT>, Args...>(upvalue_<FuncType>(L), L)); 
			else
#endif
				push<RVal>(L, direct_invoke_func<1, RVal, FuncType, CT*, Args...>(upvalue_<FuncType>(L), L));
		}

		template<typename T>
		static typename std::enable_if<std::is_void<T>::value, void>::type _invoke(lua_State *L)
		{
			using FuncType = void(CT::*)(Args...);
#ifdef _ALLOW_SHAREDPTR_INVOKE
			if (CheckSameMetaTable(L, 1, get_class_name<CT>()) == false)
				direct_invoke_func<1, void, FuncType, std::shared_ptr<CT>, Args...>(upvalue_<FuncType>(L), L); 
			else
#endif
				direct_invoke_func<1, void, FuncType, CT*, Args...>(upvalue_<FuncType>(L), L);

		}

		static int invoke_function(lua_State *L)
		{
			CHECK_CLASS_PTR(CT);
			TRY_LUA_TINKER_INVOKE()
			{
				_invoke_function<RVal>(L);
				return 1;
			}
			CATCH_LUA_TINKER_INVOKE()
			{
				lua_pushfstring(L, "lua fail to invoke functor");
				lua_error(L);
			}
			return 0;

		}

		template<typename T>
		static typename std::enable_if<!std::is_void<T>::value, void>::type _invoke_function(lua_State *L)
		{
			using FuncWarpType = member_functor<CT, RVal, Args...>;
			FuncWarpType* pFuncWarp = upvalue_<FuncWarpType*>(L);
			push<RVal>(L, direct_invoke_func<1, RVal, FunctionType, CT*, Args...>(std::forward<FunctionType>(pFuncWarp->m_func), L));
		}

		template<typename T>
		static typename std::enable_if<std::is_void<T>::value, void>::type _invoke_function(lua_State *L)
		{
			using FuncWarpType = member_functor<CT, void, Args...>;
			FuncWarpType* pFuncWarp = upvalue_<FuncWarpType*>(L);
			
			direct_invoke_func<1, void, FunctionType, CT*, Args...>(std::forward<FunctionType>(pFuncWarp->m_func), L);
		}
	};

	
	template <typename RVal, typename ... Args>
	struct functor
	{
		typedef std::function< RVal(Args...) > FunctionType;
		FunctionType m_func;

		functor(FunctionType func)
			:m_func(func)
		{}

		~functor()
		{}

		static int invoke(lua_State *L)
		{
			TRY_LUA_TINKER_INVOKE()
			{
				_invoke<RVal>(L);
				return 1;
			}
			CATCH_LUA_TINKER_INVOKE()
			{
				lua_pushfstring(L, "lua fail to invoke functor");
				lua_error(L);
			}
			return 0;
		}

		template<typename T>
		static typename std::enable_if<!std::is_void<T>::value, void>::type _invoke(lua_State *L)
		{
			using FuncType = RVal(*)(Args...);
			push<RVal>(L, direct_invoke_func<1, RVal, FuncType,  Args...>(std::forward<FuncType>(upvalue_<FuncType>(L)), L));
		}

		template<typename T>
		static typename std::enable_if<std::is_void<T>::value, void>::type _invoke(lua_State *L)
		{
			using FuncType = void(*)(Args...);
			direct_invoke_func<1, void, FuncType, Args...>(std::forward<FuncType>(upvalue_<FuncType>(L)), L);
		}


		static int invoke_function(lua_State *L)
		{
			TRY_LUA_TINKER_INVOKE()
			{
				_invoke_function<RVal>(L);
				return 1;
			}
			CATCH_LUA_TINKER_INVOKE()
			{
				lua_pushfstring(L, "lua fail to invoke functor");
				lua_error(L);
			}
			return 0;
		}

		template<typename T>
		static typename std::enable_if<!std::is_void<T>::value, void>::type _invoke_function(lua_State *L)
		{
			using FuncWarpType = functor<RVal, Args...>;
			FuncWarpType* pFuncWarp = upvalue_<FuncWarpType*>(L);
			push<RVal>(L, direct_invoke_func<1, RVal, FunctionType, Args...>(std::forward<FunctionType>(pFuncWarp->m_func), L));
		}

		template<typename T>
		static typename std::enable_if<std::is_void<T>::value, void>::type _invoke_function(lua_State *L)
		{
			using FuncWarpType = functor<void, Args...>;
			FuncWarpType* pFuncWarp = upvalue_<FuncWarpType*>(L);

			direct_invoke_func<1, void, FunctionType, Args...>(std::forward<FunctionType>(pFuncWarp->m_func), L);
		}
	};


	// member variable
	struct var_base
	{
		virtual ~var_base() {};
		virtual void get(lua_State *L) = 0;
		virtual void set(lua_State *L) = 0;
	};

	template<typename T, typename V>
	struct mem_var : var_base
	{
		V T::*_var;
		mem_var(V T::*val) : _var(val) {}
		void get(lua_State *L) 
		{ 
			CHECK_CLASS_PTR(T); 
#ifdef _ALLOW_SHAREDPTR_INVOKE
			if (CheckSameMetaTable(L, 1, get_class_name<T>()) == false)
				push(L, read<std::shared_ptr<T>>(L, 1).get()->*(_var));
			else
#endif
				push(L, read<T*>(L, 1)->*(_var));
		}
		void set(lua_State *L) 
		{ 
			CHECK_CLASS_PTR(T); 
#ifdef _ALLOW_SHAREDPTR_INVOKE
			if (CheckSameMetaTable(L, 1, get_class_name<T>()) == false)
				read<std::shared_ptr<T>>(L, 1).get()->*(_var) = read<V>(L, 3);
			else
#endif
				read<T*>(L, 1)->*(_var) = read<V>(L, 3);
		}
	};

	// constructor
	template<typename T, typename ...Args>
	struct constructor
	{
		static int invoke(lua_State *L)
		{
			new(lua_newuserdata(L, sizeof(val2user<T>))) val2user<T>(L, class_tag<Args...>());
			push_meta(L, get_class_name<T>());
			lua_setmetatable(L, -2);

			return 1;
		}
	};


	template<typename T>
	struct constructor<T>
	{
		static int invoke(lua_State *L)
		{
			new(lua_newuserdata(L, sizeof(val2user<T>))) val2user<T>();
			push_meta(L, get_class_name<T>());
			lua_setmetatable(L, -2);

			return 1;
		}
	};


	// destroyer
	template<typename T>
	int destroyer(lua_State *L)
	{
		((T*)lua_touserdata(L, 1))->~T();
		return 0;
	}
	int destroyer_shared_ptr(lua_State *L);

	// global function
	template<typename R,typename ...ARGS>
	void def(lua_State* L, const char* name, R(func)(ARGS...) )
	{
		using Functor_Warp = functor<R,ARGS...>;
		lua_pushlightuserdata(L, (void*)func);
		lua_pushcclosure(L, &Functor_Warp::invoke, 1);
		lua_setglobal(L, name);
	}

	template<typename R, typename ...ARGS>
	void def(lua_State* L, const char* name, std::function<R(ARGS...)> func )
	{
		using Functor_Warp = functor<R, ARGS...>;

		new(lua_newuserdata(L, sizeof(Functor_Warp))) Functor_Warp(func);
		//register functor
		{
			lua_newtable(L);
			lua_pushstring(L, "__gc");
			lua_pushcclosure(L, &destroyer<Functor_Warp>, 0);
			lua_rawset(L, -3);
			lua_setmetatable(L, -2);
		}

		lua_pushcclosure(L, &Functor_Warp::invoke_function, 1);
		lua_setglobal(L, name);

	}

	// global variable
	template<typename T>
	void set(lua_State* L, const char* name, T object)
	{
		push(L, object);
		lua_setglobal(L, name);
	}

	template<typename T>
	T get(lua_State* L, const char* name)
	{
		lua_getglobal(L, name);
		return pop<T>(L);
	}

	template<typename T>
	void decl(lua_State* L, const char* name, T object)
	{
		set(L, name, object);
	}

	// call lua func
	template<typename RVal>
	RVal call(lua_State* L, const char* name)
	{
		lua_pushcclosure(L, on_error, 0);
		int errfunc = lua_gettop(L);

		lua_getglobal(L, name);
		if (lua_isfunction(L, -1))
		{
			lua_pcall(L, 0, 1, errfunc);
		}
		else
		{
			print_error(L, "lua_tinker::call() attempt to call global `%s' (not a function)", name);
		}

		lua_remove(L, errfunc);
		return pop<RVal>(L);
	}

	template<typename RVal, typename ...Args>
	RVal call(lua_State* L, const char* name, Args... arg)
	{

		lua_pushcclosure(L, on_error, 0);
		int errfunc = lua_gettop(L);

		lua_getglobal(L, name);
		if (lua_isfunction(L, -1))
		{
			push_args(L, arg...);

			if (lua_pcall(L, sizeof...(Args), 1, errfunc) != 0)
			{
				lua_pop(L, 1);
			}
		}
		else
		{
			print_error(L, "lua_tinker::call() attempt to call global `%s' (not a function)", name);
		}

		lua_remove(L, -2);
		return pop<RVal>(L);
	}


	// class init
	template<typename T>
	void class_add(lua_State* L, const char* name, bool bInitShared = false)
	{
		class_name<T>::name(name);
		lua_newtable(L);

		lua_pushstring(L, "__name");
		lua_pushstring(L, name);
		lua_rawset(L, -3);

		lua_pushstring(L, "__index");
		lua_pushcclosure(L, meta_get, 0);
		lua_rawset(L, -3);

		lua_pushstring(L, "__newindex");
		lua_pushcclosure(L, meta_set, 0);
		lua_rawset(L, -3);

		lua_pushstring(L, "__gc");
		lua_pushcclosure(L, destroyer<UserDataWapper>, 0);
		lua_rawset(L, -3);

		lua_setglobal(L, name);

		if (bInitShared)
		{
			std::string strSharedName = (std::string(name) + S_SHARED_PTR_NAME);
			class_name< std::shared_ptr<T> >::name(strSharedName.c_str());
			lua_newtable(L);

			lua_pushstring(L, "__name");
			lua_pushstring(L, strSharedName.c_str());
			lua_rawset(L, -3);

			lua_pushstring(L, "__gc");
			lua_pushcclosure(L, destroyer_shared_ptr, 0);
			lua_rawset(L, -3);

#ifdef _ALLOW_SHAREDPTR_INVOKE
			lua_pushstring(L, "__index");
			lua_pushcclosure(L, meta_get, 0);
			lua_rawset(L, -3);

			lua_pushstring(L, "__newindex");
			lua_pushcclosure(L, meta_set, 0);
			lua_rawset(L, -3);

			lua_pushstring(L, "__parent");
			push_meta(L, name);
			lua_rawset(L, -3);
#endif
			lua_setglobal(L, strSharedName.c_str());
		}

	}

	// Tinker Class Inheritence
	template<typename T, typename P>
	void class_inh(lua_State* L)
	{
		push_meta(L, get_class_name<T>());
		if (lua_istable(L, -1))
		{
			lua_pushstring(L, "__parent");
			push_meta(L, get_class_name<P>());
			lua_rawset(L, -3);
		}
		lua_pop(L, 1);
	}

	// Tinker Class Constructor
	template<typename T, typename F>
	void class_con(lua_State* L, F func)
	{
		push_meta(L, get_class_name<T>());
		if (lua_istable(L, -1))
		{
			lua_newtable(L);
			lua_pushstring(L, "__call");
			lua_pushcclosure(L, func, 0);
			lua_rawset(L, -3);
			lua_setmetatable(L, -2);
		}
		lua_pop(L, 1);
	}

	// Tinker Class Functions
	template<typename T, typename R, typename ...ARGS>
	void class_def(lua_State* L, const char* name, R(T::*func)(ARGS...) )
	{
		push_meta(L, get_class_name<T>());
		if (lua_istable(L, -1))
		{
			using Functor_Warp = member_functor<T,R,ARGS...>;
			using FunctionType = R(T::*)(ARGS...);
			//register functor
			lua_pushstring(L, name);
			new(lua_newuserdata(L, sizeof(FunctionType))) FunctionType(func);
			lua_pushcclosure(L, &Functor_Warp::invoke, 1);
			lua_rawset(L, -3);
		}
		lua_pop(L, 1);
	}
	template<typename T, typename R, typename ...ARGS>
	void class_def(lua_State* L, const char* name, R(T::*func)(ARGS...) const)
	{
		push_meta(L, get_class_name<T>());
		if (lua_istable(L, -1))
		{
			using Functor_Warp = member_functor<T, R, ARGS...>;
			using FunctionType = R(T::*)(ARGS...) const;
			//register functor
			lua_pushstring(L, name);
			new(lua_newuserdata(L, sizeof(FunctionType))) FunctionType(func);
			lua_pushcclosure(L, &Functor_Warp::invoke, 1);
			lua_rawset(L, -3);
		}
		lua_pop(L, 1);
	}

	template<typename T, typename R, typename ...ARGS>
	void class_def(lua_State* L, const char* name, std::function<R(T*,ARGS...)> func)
	{
		push_meta(L, get_class_name<T>());
		if (lua_istable(L, -1))
		{
			using Functor_Warp = member_functor<T, R, ARGS...>;
			lua_pushstring(L, name);
			new(lua_newuserdata(L, sizeof(Functor_Warp))) Functor_Warp(func);
			//register metatable for gc
			{
				lua_newtable(L);
				lua_pushstring(L, "__gc");
				lua_pushcclosure(L, &destroyer<Functor_Warp>, 0);
				lua_rawset(L, -3);

				lua_setmetatable(L, -2);
			}

			lua_pushcclosure(L, &Functor_Warp::invoke_function, 1);
			lua_rawset(L, -3);



		}
		lua_pop(L, 1);
	}
	


	// Tinker Class Variables
	template<typename T, typename BASE, typename VAR>
	void class_mem(lua_State* L, const char* name, VAR BASE::*val)
	{
		push_meta(L, get_class_name<T>());
		if (lua_istable(L, -1))
		{
			lua_pushstring(L, name);
			new(lua_newuserdata(L, sizeof(mem_var<BASE, VAR>))) mem_var<BASE, VAR>(val);
			lua_rawset(L, -3);
		}
		lua_pop(L, 1);
	}

	

	// Table Object on Stack
	struct table_obj
	{
		table_obj(lua_State* L, int index);
		~table_obj();

		void inc_ref();
		void dec_ref();

		bool validate();

		template<typename T>
		void set(const char* name, T object)
		{
			if (validate())
			{
				lua_pushstring(m_L, name);
				push(m_L, object);
				lua_settable(m_L, m_index);
			}
		}

		template<typename T>
		T get(const char* name)
		{
			if (validate())
			{
				lua_pushstring(m_L, name);
				lua_gettable(m_L, m_index);
			}
			else
			{
				lua_pushnil(m_L);
			}

			return pop<T>(m_L);
		}

		template<typename T>
		T get(int num)
		{
			if (validate())
			{
				lua_pushinteger(m_L, num);
				lua_gettable(m_L, m_index);
			}
			else
			{
				lua_pushnil(m_L);
			}

			return pop<T>(m_L);
		}

		lua_State*      m_L;
		int             m_index;
		const void*     m_pointer;
		int             m_ref;
	};

	// Table Object Holder
	struct table
	{
		table(lua_State* L);
		table(lua_State* L, int index);
		table(lua_State* L, const char* name);
		table(const table& input);
		~table();

		template<typename T>
		void set(const char* name, T object)
		{
			m_obj->set(name, object);
		}

		template<typename T>
		T get(const char* name)
		{
			return m_obj->get<T>(name);
		}

		template<typename T>
		T get(int num)
		{
			return m_obj->get<T>(num);
		}

		table_obj*      m_obj;
	};

} // namespace lua_tinker

typedef lua_tinker::table LuaTable;

#endif //_LUA_TINKER_H_
