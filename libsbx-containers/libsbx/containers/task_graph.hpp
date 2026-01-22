// SPDX-License-Identifier: MIT
// #ifndef LIBSBX_CONTAINERS_TASK_GRAPH_HPP_
// #define LIBSBX_CONTAINERS_TASK_GRAPH_HPP_

// #include <cstdint>

// #include <string>
// #include <variant>
// #include <vector>
// #include <memory>
// #include <functional>
// #include <algorithm>

// namespace sbx::containers {

// namespace detail {

// enum class node_state : std::uint32_t {
//   none           = 0x00000000,
//   conditioned    = 0x10000000,
//   preempted      = 0x20000000,
//   retain_subflow = 0x40000000,
//   joined_subflow = 0x80000000
// }; // enum class node_state

// enum class error_state : std::uint32_t {
//   none      = 0x00000000,
//   exception = 0x10000000,
//   cancelled = 0x20000000,
//   anchored  = 0x40000000
// }; // enum class node_state

// template<typename T, typename>
// struct get_index;

// template<std::size_t I, typename... Ts>
// struct get_index_impl {};

// template<std::size_t I, typename T, typename... Ts>
// struct get_index_impl<I, T, T, Ts...> : std::integral_constant<std::size_t, I>{};

// template<std::size_t I, typename T, typename U, typename... Ts>
// struct get_index_impl<I, T, U, Ts...> : get_index_impl<I+1u, T, Ts...>{};

// template<typename T, typename... Ts>
// struct get_index<T, std::variant<Ts...>> : get_index_impl<0u, T, Ts...>{};

// template<typename T, typename... Ts>
// constexpr auto get_index_v = get_index<T, Ts...>::value;

// class graph_node;
// class graph_base;
// class graph_builder;
// class task;
// class sub_graph;

// class graph_base : std::vector<std::unique_ptr<graph_node>> {

//   friend class graph_node;
//   friend class graph_builder;

//   using base = std::vector<std::unique_ptr<graph_node>>;

// public:

//   graph_base() = default;

//   graph_base(const graph_base& other) = delete;

//   graph_base(graph_base&& other) = default;

//   auto operator=(const graph_base& other) -> graph_base& = delete;

//   auto operator=(graph_base&& other) -> graph_base& = default;

// private:

//   auto _reserve(const std::size_t capacity) -> void;

//   auto _erase(graph_node* node) -> void;

//   template<typename... Args>
//   auto _emplace_back(Args&&... args) -> graph_node*;

// }; // class graph_base

// struct task_parameters {
//   std::string name;
//   void*data ;
// }; // struct task_parameters

// struct default_task_parameters { };

// template<typename Type>
// constexpr bool is_task_params_v = std::is_same_v<std::decay_t<Type>, task_parameters> || std::is_same_v<std::decay_t<Type>, default_task_parameters> || std::is_constructible_v<std::string, Type>;

// template<typename Callable, typename = void>
// struct is_static_task : std::false_type{ };

// template<typename Callable>
// struct is_static_task<Callable, std::enable_if_t<std::is_invocable_v<Callable>>> : std::is_same<std::invoke_result_t<Callable>, void> { };

// template <typename Callable>
// constexpr bool is_static_task_v = is_static_task<Callable>::value;

// class graph_node {
  
//   friend class graph_builder;
//   friend class task;

//   using placeholder_task = std::monostate;
  
//   struct static_task {
    
//     template<typename Callable>
//     static_task(Callable&& callable);
    
//     std::function<void()> work;
//   }; // struct static_task

//   struct sub_graph {
    
//     template<typename Callable>
//     sub_graph(Callable&& callable);
    
//     std::function<void(sub_graph&)> work;
//     graph_base graph;
//   }; // struct sub_graph
  
  
//   using task_handle = std::variant<placeholder_task, static_task>;

// public:

//   inline static constexpr auto placeholder = get_index_v<placeholder_task, task_handle>;
//   inline static constexpr auto static_work = get_index_v<static_task, task_handle>;

//   graph_node();
  
//   template<typename... Args>
//   graph_node(node_state node_state, const task_parameters& parameters, graph_node* parent, Args&&...);
  
//   template<typename... Args>
//   graph_node(node_state node_state,const default_task_parameters& parameters, graph_node* parent, Args&&...);

//   auto num_successors() const -> std::size_t;
//   auto num_predecessors() const -> std::size_t;

//   auto name() const -> const std::string&;
  

// private:

//   auto _precede(graph_node* node) -> void;
//   auto _remove_successors(graph_node* node) -> void;
//   auto _remove_predecessors(graph_node* node) -> void;

//   node_state _state;
//   std::string _name;
//   void* _data;
//   graph_node* _parent;
//   std::size_t _num_successors;
//   std::vector<graph_node*> _edges;
//   task_handle _handle;

// }; // class graph_node

// class task {

//   friend class graph_builder;

// public:

//   auto name() const -> const std::string&;

//   auto num_predecessors() const -> std::size_t;
//   auto num_successors() const -> std::size_t;

//   template<typename... Tasks>
//   auto precede(Tasks&&... tasks) -> task&;

//   template<typename... Tasks>
//   auto succeed(Tasks&&... tasks) -> task&;

// private:

//   task(graph_node* node);

//   graph_node* _node;

// }; // class task

// class graph_builder {

// public:

//   graph_builder(graph_base& graph);

//   template <typename Callable>
//   requires (is_static_task_v<Callable>)
//   auto emplace(Callable&& callable) -> task;

//   template<typename... Callables>
//   requires (sizeof...(Callables) > 1u)
//   auto emplace(Callables&&... callables) -> decltype(auto);

// protected:

//   graph_base& _graph;

// private:

// }; // class graph_builder

// class sub_graph : public graph_builder {

// public:

// private:

//   sub_graph(graph_node* parent, graph_base& graph)
//   : graph_builder{graph},
//     _parent{parent} { }

//   graph_node* _parent;

// }; // class subgraph

// auto graph_base::_reserve(const std::size_t capacity) -> void {
//   base::reserve(capacity);
// }

// auto graph_base::_erase(graph_node* node) -> void {
//   base::erase(std::remove_if(base::begin(), base::end(), [&](auto& entry){ return entry.get() == node; }), base::end() );
// } 

// template<typename... Args>
// auto graph_base::_emplace_back(Args&&... args) -> graph_node* {
//   base::push_back(std::make_unique<graph_node>(std::forward<Args>(args)...));
//   return back().get();
// }

// template<typename Callable>
// graph_node::static_task::static_task(Callable&& callable)
// : work{std::forward<Callable>(callable)} { }

// template<typename Callable>
// graph_node::sub_graph::sub_graph(Callable&& callable)
// : work{std::forward<Callable>(callable)} { }

// graph_node::graph_node()
// : _state{node_state::none},
//   _data{nullptr},
//   _parent{nullptr},
//   _num_successors{0u} { }

// template<typename... Args>
// graph_node::graph_node(node_state node_state, const task_parameters& parameters, graph_node* parent, Args&&... args)
// : _state{node_state},
//   _name{parameters.name},
//   _data{parameters.data},
//   _parent{parent},
//   _num_successors{0u},
//   _handle{std::forward<Args>(args)...} { }

// template<typename... Args>
// graph_node::graph_node(node_state node_state, const default_task_parameters& parameters, graph_node* parent, Args&&...args)
// : _state{node_state},
//   _data{nullptr},
//   _parent{parent},
//   _num_successors{0u},
//   _handle{std::forward<Args>(args)...} { }

// auto graph_node::num_successors() const -> std::size_t {
//   return _num_successors;
// }

// auto graph_node::num_predecessors() const -> std::size_t {
//   return _edges.size() - _num_successors;
// }

// auto graph_node::name() const -> const std::string& {
//   return _name;
// }

// auto graph_node::_precede(graph_node* node) -> void {
//   _edges.push_back(node);
//   std::swap(_edges[_num_successors++], _edges[_edges.size() - 1]);
//   node->_edges.push_back(this);
// }

// auto graph_node::_remove_successors(graph_node* node) -> void {
//   auto sit = std::remove(_edges.begin(), _edges.begin() + _num_successors, node);
//   size_t new_num_successors = std::distance(_edges.begin(), sit);
//   std::move(_edges.begin() + _num_successors, _edges.end(), sit);
//   _edges.resize(_edges.size() - (_num_successors - new_num_successors));
//   _num_successors = new_num_successors;
// }

// auto graph_node::_remove_predecessors(graph_node* node) -> void {
//   _edges.erase(std::remove(_edges.begin() + _num_successors, _edges.end(), node), _edges.end());
// }

// auto task::name() const -> const std::string& {
//   return _node->name();
// }

// auto task::num_predecessors() const -> std::size_t {
//   return _node->num_predecessors();
// }

// auto task::num_successors() const -> std::size_t {
//   return _node->num_predecessors();
// }

// template<typename... Tasks>
// auto task::precede(Tasks&&... tasks) -> task& {
//   (_node->_precede(tasks._node), ...);
//   return *this;
// }

// template<typename... Tasks>
// auto task::succeed(Tasks&&... tasks) -> task& {
//   (tasks._node->_precede(_node), ...);
//   return *this;
// }

// task::task(graph_node* node)
// : _node{node} { }

// graph_builder::graph_builder(graph_base& graph)
// : _graph{graph} { }

// template <typename Callable>
// requires (is_static_task_v<Callable>)
// auto graph_builder::emplace(Callable&& callable) -> task {
//   return task{_graph._emplace_back(node_state::none, default_task_parameters{}, nullptr, std::in_place_type_t<graph_node::static_task>{}, std::forward<Callable>(callable) )};
// }

// template<typename... Callables>
// requires (sizeof...(Callables) > 1u)
// auto graph_builder::emplace(Callables&&... callables) -> decltype(auto) {
//   _graph.reserve(sizeof...(Callables));
//   return std::make_tuple(emplace(std::forward<Callables>(callables))...);
// }

// } // namespace detail 

// class task_graph : public detail::graph_builder {

//   using base = detail::graph_builder; 

// public:

//   task_graph(const std::string& name)
//   : base{_graph},
//     _name{name} { }

// private:

//   detail::graph_base _graph;
//   std::string _name;

// }; // class graph


// } // namespace sbx::containers

// #endif // LIBSBX_CONTAINERS_TASK_GRAPH_HPP_
