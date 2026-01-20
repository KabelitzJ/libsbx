// #ifndef DEMO_TERRAIN_VORONOI_HPP_
// #define DEMO_TERRAIN_VORONOI_HPP_

// #include <vector>
// #include <memory>
// #include <list>
// #include <array>

// #include <libsbx/utility/enum.hpp>

// #include <libsbx/memory/observer_ptr.hpp>

// #include <libsbx/math/vector2.hpp>

// namespace demo {

// template<typename Type>
// concept priority_queue_element = requires(Type t) {
//   { t.index() } -> std::same_as<std::int32_t&>;
//   { t < t } -> std::same_as<bool>;
// }; // concept priority_queue_element

// template<priority_queue_element Type>
// class priority_queue {

// public:

//   using value_type = Type;

//   priority_queue() {

//   }

//   auto is_empty() const -> bool {
//     return _elements.empty();
//   }

//   auto pop() -> std::unique_ptr<value_type> {
//     if (_elements.empty()) {
//       return nullptr;
//     }

//     _swap(0, _elements.size() - 1u);
//     auto top = std::move(_elements.back());
//     _elements.pop_back();
//     _shift_down(0u);

//     return top;
//   }

//   auto push(std::unique_ptr<value_type> element) -> void {
//     element->index() = _elements.size();
//     _elements.push_back(std::move(element));
//     _shift_up(_elements.size() - 1u);
//   }

//   auto update(const std::size_t index) -> void {
//     const auto parent = _parent(index);

//     if (parent >= 0 && *_elements[parent] < *_elements[index]) {
//       _shift_up(index);
//     } else {
//       _shift_down(index);
//     }
//   }

//   auto remove(const std::size_t index) -> void {
//     _swap(index, _elements.size() - 1u);
//     _elements.pop_back();
    
//     if (index < _elements.size()) {
//       update(index);
//     }
//   }

// private:

//   auto _parent(const std::size_t index) const -> std::int32_t {
//     return (index + 1u) / 2u - 1u;
//   }

//   auto _left_child(const std::size_t index) const -> std::size_t {
//     return 2u * (index + 1u) - 1u;
//   }

//   auto _right_child(const std::size_t index) const -> std::size_t {
//     return 2u * (index + 1u);
//   }

//   auto _shift_down(const std::size_t index) -> void {
//     const auto left = _left_child(index);
//     const auto right = _right_child(index);
//     auto largest = index;

//     if (left < _elements.size() && *_elements[largest] < *_elements[left]) {
//       largest = left;
//     }

//     if (right < _elements.size() && *_elements[largest] < *_elements[right]) {
//       largest = right;
//     }

//     if (largest != index) {
//       _swap(index, largest);
//       _shift_down(largest);
//     }
//   }

//   auto _shift_up(const std::size_t index) -> void {
//     const auto parent = _parent(index);

//     if (parent >= 0 && *_elements[parent] < *_elements[index]) {
//       _swap(index, parent);
//       _shift_up(parent);
//     }
//   }

//   auto _swap(const std::size_t left, const std::size_t right) -> void {
//     auto temp = std::move(_elements[left]);
//     _elements[left] = std::move(_elements[right]);
//     _elements[right] = std::move(temp);

//     _elements[left]->index() = left;
//     _elements[right]->index() = right;
//   }

//   std::vector<std::unique_ptr<value_type>> _elements;

// }; // class priority_queue

// struct face;
// struct site;
// struct half_edge;
// struct arc;

// struct site {
//   std::size_t index;
//   sbx::math::vector2 position;
//   demo::face* face;
// }; // struct site

// struct point {
//   sbx::math::vector2 position;
//   std::list<demo::point>::iterator iterator;
// }; // struct point

// struct half_edge {
//   demo::point* start;
//   demo::point* end;
//   demo::half_edge* twin;
//   demo::face* face;
//   demo::half_edge* previous;
//   demo::half_edge* next;
//   std::list<half_edge>::iterator iterator;
// }; // struct half_edge

// struct face {
//   demo::site* site;
//   demo::half_edge* edge;
// }; // struct face

// struct site_event {

//   std::float_t y;
//   std::int32_t index;
//   demo::site* site;

//   site_event(demo::site* s)
//   : y{s->position.y()},
//     site{s},
//     index{-1} { }
  
// }; // struct site_event

// struct circle_event {

//   std::float_t y;
//   std::int32_t index;
//   sbx::math::vector2 center;
//   demo::arc* arc;

//   circle_event(const std::float_t y_position, const sbx::math::vector2& c, demo::arc* a)
//   : y{y_position},
//     center{c},
//     arc{a},
//     index{-1} { }

// }; // struct circle_event

// class event {

// public:

//   event(const site_event& e)
//   : _event{e} { }

//   event(const circle_event& e)
//   : _event{e} { }

//   auto y() const -> const std::float_t& {
//     if (is_site_event()) {
//       return site_event().y;
//     } else {
//       return circle_event().y;
//     }
//   }

//   auto y() -> std::float_t& {
//     if (is_site_event()) {
//       return site_event().y;
//     } else {
//       return circle_event().y;
//     }
//   }

//   auto index() -> std::int32_t& {
//     if (is_site_event()) {
//       return site_event().index;
//     } else {
//       return circle_event().index;
//     }
//   }

//   auto is_site_event() const -> bool {
//     return std::holds_alternative<demo::site_event>(_event);
//   }

//   auto site_event() const -> const demo::site_event& {
//     return std::get<demo::site_event>(_event);
//   }

//   auto site_event() -> demo::site_event& {
//     return std::get<demo::site_event>(_event);
//   }

//   auto is_circle_event() const -> bool {
//     return std::holds_alternative<demo::circle_event>(_event);
//   }

//   auto circle_event() const -> const demo::circle_event& {
//     return std::get<demo::circle_event>(_event);
//   }

//   auto circle_event() -> demo::circle_event& {
//     return std::get<demo::circle_event>(_event);
//   }

// private:

//   std::variant<demo::site_event, demo::circle_event> _event;

// }; // class event

// auto operator<(const event& left, const event& right) -> bool {
//   return left.y() < right.y();
// }

// struct arc {
 
//   enum class color : std::uint8_t {
//     red,
//     black
//   }; // enum class color

//   demo::arc* parent;
//   demo::arc* left;
//   demo::arc* right;

//   demo::site* site;
//   demo::half_edge* left_half_edge;
//   demo::half_edge* right_half_edge;
//   demo::event* event;

//   demo::arc* previous;
//   demo::arc* next;

//   color color;

// }; // struct arc

// struct box {

//   inline static constexpr auto epsilon = std::numeric_limits<std::float_t>::epsilon();

//   enum class side : std::uint8_t {
//     top,
//     right,
//     bottom,
//     left
//   }; // enum class side

//   struct intersection {
//     box::side side;
//     sbx::math::vector2 position;
//   }; // struct intersection

//   std::float_t left;
//   std::float_t right;
//   std::float_t top;
//   std::float_t bottom;

//   auto contains(const sbx::math::vector2& point) const -> bool {
//     return point.x() >= left - epsilon && point.x() <= right + epsilon && point.y() >= top - epsilon && point.y() <= bottom + epsilon;
//   }

//   auto first_intersection(const sbx::math::vector2& start, const sbx::math::vector2& direction) const -> intersection {
//     auto result = intersection{};

//     auto t = std::numeric_limits<std::float_t>::infinity();

//     if (direction.x() > 0.0f) {
//       t = (right - start.x()) / direction.x();
//       result.side = side::right;
//       result.position = start + direction * t;
//     } else if (direction.x() < 0.0f) {
//       t = (left - start.x()) / direction.x();
//       result.side = side::left;
//       result.position = start + direction * t;
//     }

//     if (direction.y() > 0.0f) {
//       auto new_t = (top - start.y()) / direction.y();

//       if (new_t < t) {
//         result.side = side::top;
//         result.position = start + direction * new_t;
//       }
//     } else if (direction.y() < 0.0f) {
//       auto new_t = (bottom - start.y()) / direction.y();

//       if (new_t < t) {
//         result.side = side::bottom;
//         result.position = start + direction * new_t;
//       }
//     }

//     return result;
//   }

//   auto intersections(const sbx::math::vector2& start, const sbx::math::vector2& end) const -> std::vector<intersection> {
//     auto result = std::vector<intersection>{};

//     const auto direction = end - start;
//     auto t = std::array<std::float_t, 2u>{};

//     // Left
//     if (start.x() < left || end.x() < left - epsilon) {
//       const auto i = result.size();

//       t[i] = (left - start.x()) / direction.x();

//       if (t[i] > epsilon && t[i] < 1.0 - epsilon) {
//         const auto position = start + direction * t[i];

//         if (position.y() >= bottom - epsilon && position.y() <= top + epsilon) {
//           result.push_back(intersection{side::left, position});
//         }
//       }
//     }

//     // Right
//     if (start.x() > right || end.x() > right + epsilon) {
//       const auto i = result.size();

//       t[i] = (right - start.x()) / direction.x();

//       if (t[i] > epsilon && t[i] < 1.0 - epsilon) {
//         const auto position = start + direction * t[i];

//         if (position.y() >= bottom - epsilon && position.y() <= top + epsilon) {
//           result.push_back(intersection{side::right, position});
//         }
//       }
//     }

//     // Bottom
//     if (start.y() < bottom || end.y() < bottom - epsilon) {
//       const auto i = result.size();

//       t[i] = (bottom - start.y()) / direction.y();

//       if (t[i] > epsilon && t[i] < 1.0 - epsilon) {
//         const auto position = start + direction * t[i];

//         if (position.x() >= left - epsilon && position.x() <= right + epsilon) {
//           result.push_back(intersection{side::bottom, position});
//         }
//       }
//     }

//     // Top
//     if (start.y() > top || end.y() > top + epsilon) {
//       const auto i = result.size();

//       t[i] = (top - start.y()) / direction.y();

//       if (t[i] > epsilon && t[i] < 1.0 - epsilon) {
//         const auto position = start + direction * t[i];

//         if (position.x() >= left - epsilon && position.x() <= right + epsilon) {
//           result.push_back(intersection{side::top, position});
//         }
//       }
//     }

//     // Sort the intersections by distance
//     if (result.size() == 2u && t[0] > t[1]) {
//       std::swap(result[0], result[1]);
//     }

//     return result;
//   }

// }; // class box

// class beach_line {

// public:

//   beach_line()
//   : _nil{new arc{}},
//     _root{_nil} {
//     _nil->color = arc::color::black;
//   }

//   beach_line(const beach_line&) = delete;
//   beach_line(beach_line&&) = delete;

//   auto operator=(const beach_line&) -> beach_line& = delete;
//   auto operator=(beach_line&&) -> beach_line& = delete;

//   auto create_arc(site* site) -> arc* {
//     return new arc{_nil, _nil, _nil, site, nullptr, nullptr, nullptr, _nil, _nil, arc::color::red};
//   }

//   auto is_empty() const -> bool {
//     return is_nil(_root);
//   }

//   auto is_nil(arc* node) const -> bool {
//     return node == _nil;
//   }

//   auto set_root(arc* node) -> void {
//     _root = node;
//     _root->color = arc::color::black;
//   }

//   auto left_most_arc() -> arc* {
//     auto node = _root;

//     while (!is_nil(node->previous)) {
//       node = node->previous;
//     }

//     return node;
//   }

//   auto arc_above(const sbx::math::vector2& point, const std::float_t y) -> arc* {
//     auto node = _root;
//     auto found = false;

//     while (!found) {
//       auto left_breakpoint = -std::numeric_limits<std::float_t>::infinity();
//       auto right_breakpoint = std::numeric_limits<std::float_t>::infinity();

//       if (!is_nil(node->previous)) {
//         left_breakpoint = _break_point(node->previous->site->position, node->site->position, y);
//       }

//       if (!is_nil(node->next)) {
//         right_breakpoint = _break_point(node->site->position, node->next->site->position, y);
//       }

//       if (point.x() < left_breakpoint) {
//         node = node->left;
//       } else if (point.x() > right_breakpoint) {
//         node = node->right;
//       } else {
//         found = true;
//       }
//     }

//     return node;
//   }

//   auto insert_before(arc* x, arc* y) -> void {
//     if (is_nil(x->left)) {
//       x->left = y;
//       y->parent = x;
//     } else {
//       x->previous->right = y;
//       y->parent = x->previous;
//     }

//     y->previous = x->previous;

//     if (!is_nil(y->previous)) {
//       y->previous->next = y;
//     }

//     y->next = x;
//     x->previous = y;

//     _insert_fixup(y);
//   }

//   auto insert_after(arc* x, arc* y) -> void {
//     if (is_nil(x->right)) {
//       x->right = y;
//       y->parent = x;
//     } else {
//       x->next->left = y;
//       y->parent = x->next;
//     }

//     y->next = x->next;

//     if (!is_nil(y->next)) {
//       y->next->previous = y;
//     }

//     y->previous = x;
//     x->next = y;

//     _insert_fixup(y);   
//   }

//   auto replace(arc* x, arc* y) -> void {
//     _transplant(x, y);

//     y->left = x->left;
//     y->right = x->right;
  
//     if (!is_nil(y->left)) {
//       y->left->parent = y;
//     }

//     if (!is_nil(y->right)) {
//       y->right->parent = y;
//     }

//     y->previous = x->previous;
//     y->next = x->next;

//     if (!is_nil(y->previous)) {
//       y->previous->next = y;
//     }

//     if (!is_nil(y->next)) {
//       y->next->previous = y;
//     }

//     y->color = x->color;
//   }

//   auto remove(arc* z) -> void {
//     auto y = z;
//     auto start_color = y->color;
//     auto* x = static_cast<arc*>(nullptr);

//     if (is_nil(z->left)) {
//       x = z->right;
//       _transplant(z, z->right);
//     } else if (is_nil(z->right)) {
//       x = z->left;
//       _transplant(z, z->left);
//     } else {
//       y = _minimum(z->right);
//       start_color = y->color;
//       x = y->right;

//       if (y->parent == z) {
//         x->parent = y;
//       } else {
//         _transplant(y, y->right);
//         y->right = z->right;
//         y->right->parent = y;
//       }

//       _transplant(z, y);
//       y->left = z->left;
//       y->left->parent = y;
//       y->color = z->color;
//     }

//     if (start_color == arc::color::black) {
//       _remove_fixup(x);
//     }

//     if (!is_nil(z->previous)) {
//       z->previous->next = z->next;
//     }

//     if (!is_nil(z->next)) {
//       z->next->previous = z->previous;
//     }
//   }

// private:

//   auto _minimum(arc* node) -> arc* {
//     auto current = node;

//     while (!is_nil(current->left)) {
//       current = current->left;
//     }

//     return current;
//   }

//   auto _transplant(arc* u, arc* v) -> void {
//     if (is_nil(u->parent)) {
//       _root = v;
//     } else if (u == u->parent->left) {
//       u->parent->left = v;
//     } else {
//       u->parent->right = v;
//     }

//     v->parent = u->parent;
//   }

//   auto _insert_fixup(arc* z) -> void{
//     while (z->parent->color == arc::color::red) {
//       if (z->parent == z->parent->parent->left) {
//         auto y = z->parent->parent->right;
//         // Case 1
//         if (y->color == arc::color::red) {
//           z->parent->color = arc::color::black;
//           y->color = arc::color::black;
//           z->parent->parent->color = arc::color::red;
//           z = z->parent->parent;
//         } else {
//           // Case 2
//           if (z == z->parent->right) {
//             z = z->parent;
//             _rotate_left(z);
//           }
//           // Case 3
//           z->parent->color = arc::color::black;
//           z->parent->parent->color = arc::color::red;
//           _rotate_right(z->parent->parent);
//         }
//       } else {
//         auto y = z->parent->parent->left;
//         // Case 1
//         if (y->color == arc::color::red) {
//           z->parent->color = arc::color::black;
//           y->color = arc::color::black;
//           z->parent->parent->color = arc::color::red;
//           z = z->parent->parent;
//         } else {
//           // Case 2
//           if (z == z->parent->left) {
//             z = z->parent;
//             _rotate_right(z);
//           }
//           // Case 3
//           z->parent->color = arc::color::black;
//           z->parent->parent->color = arc::color::red;
//           _rotate_left(z->parent->parent);
//         }
//       }
//     }
//     _root->color = arc::color::black;
//   }

//   auto _remove_fixup(arc* x) -> void {
//     while (x != _root && x->color == arc::color::black) {
//       auto* w = static_cast<arc*>(nullptr);

//       if (x == x->parent->left) {
//         w = x->parent->right;
//         // Case 1
//         if (w->color == arc::color::red) {
//           w->color = arc::color::black;
//           x->parent->color = arc::color::red;
//           _rotate_left(x->parent);
//           w = x->parent->right;
//         }
//         // Case 2
//         if (w->left->color == arc::color::black && w->right->color == arc::color::black) {
//           w->color = arc::color::red;
//           x = x->parent;
//         } else {
//           // Case 3
//           if (w->right->color == arc::color::black) {
//             w->left->color = arc::color::black;
//             w->color = arc::color::red;
//             _rotate_right(w);
//             w = x->parent->right;
//           }
//           // Case 4
//           w->color = x->parent->color;
//           x->parent->color = arc::color::black;
//           w->right->color = arc::color::black;
//           _rotate_left(x->parent);
//           x = _root;
//         }
//       } else {
//         w = x->parent->left;
//         // Case 1
//         if (w->color == arc::color::red) {
//           w->color = arc::color::black;
//           x->parent->color = arc::color::red;
//           _rotate_right(x->parent);
//           w = x->parent->left;
//         }
//         // Case 2
//         if (w->left->color == arc::color::black && w->right->color == arc::color::black) {
//           w->color = arc::color::red;
//           x = x->parent;
//         } else {
//           // Case 3
//           if (w->left->color == arc::color::black) {
//             w->right->color = arc::color::black;
//             w->color = arc::color::red;
//             _rotate_left(w);
//             w = x->parent->left;
//           }
//           // Case 4
//           w->color = x->parent->color;
//           x->parent->color = arc::color::black;
//           w->left->color = arc::color::black;
//           _rotate_right(x->parent);
//           x = _root;
//         } 
//       }
//     }

//     x->color = arc::color::black;
//   }

//   auto _rotate_left(arc* x) -> void {
//     auto y = x->right;
//     x->right = y->left;

//     if (!is_nil(y->left)) {
//       y->left->parent = x;
//     }

//     y->parent = x->parent;

//     if (is_nil(x->parent)) {
//       _root = y;
//     } else if (x->parent->left == x) {
//       x->parent->left = y;
//     } else {
//       x->parent->right = y;
//     }

//     y->left = x;
//     x->parent = y;
//   }

//   auto _rotate_right(arc* y) -> void {
//     auto x = y->left;
//     y->left = x->right;

//     if (!is_nil(x->right)) {
//       x->right->parent = y;
//     }

//     x->parent = y->parent;

//     if (is_nil(y->parent)) {
//       _root = x;
//     } else if (y->parent->left == y) {
//       y->parent->left = x;
//     } else {
//       y->parent->right = x;
//     }

//     x->right = y;
//     y->parent = x;
//   }

//   auto _break_point(const sbx::math::vector2& left, const sbx::math::vector2& right, const std::float_t y) -> std::float_t {
//     auto x1 = left.x();
//     auto y1 = left.y();
//     auto x2 = right.x();
//     auto y2 = right.y();

//     auto d1 = 1.0f / (2.0f * (y1 - y));
//     auto d2 = 1.0f / (2.0f * (y2 - y));

//     auto a = d1 - d2;
//     auto b = 2.0f * (x2 * d2 - x1 * d1);
//     auto c = (y1 * y1 + x1 * x1 - y * y) * d1 - (y2 * y2 + x2 * x2 - y * y) * d2;

//     auto delta = b * b - 4.0f * a * c;

//     return (-b + std::sqrt(delta)) / (2.0f * a);
//   }

//   arc* _nil;
//   arc* _root;

// }; // class beach_line

// class voronoi_diagram {

//   friend class fortune_algorithm;

// public:

//   voronoi_diagram(const std::vector<sbx::math::vector2>& points) {
//     _sites.reserve(points.size());
//     _faces.reserve(points.size());

//     for (auto i : std::views::iota(0u, points.size())) {
//       _sites.push_back(site{i, points[i], nullptr});
//       _faces.push_back(face{&_sites.back(), nullptr});
//       _sites.back().face = &_faces.back();
//     }
//   }

//   voronoi_diagram(const voronoi_diagram&) = delete;
//   voronoi_diagram(voronoi_diagram&&) = delete;

//   auto operator=(const voronoi_diagram&) -> voronoi_diagram& = delete;
//   auto operator=(voronoi_diagram&&) -> voronoi_diagram& = delete;

//   auto sites() const -> const std::vector<site>& {
//     return _sites;
//   }

//   auto sites() -> std::vector<site>& {
//     return _sites;
//   }

//   auto faces() const -> const std::vector<face>& {
//     return _faces;
//   }

//   auto faces() -> std::vector<face>& {
//     return _faces;
//   }

//   auto points() const -> const std::list<point>& {
//     return _points;
//   }

//   auto half_edges() const -> const std::list<half_edge>& {
//     return _half_edges;
//   }

//   auto intersect(const box& box) -> bool {
//     auto error = false;
//     auto processed_half_edges = std::unordered_set<half_edge*>{};
//     auto points_to_remove = std::unordered_set<demo::point*>{};

//     for (const auto& site : _sites) {
//       auto* half_edge = site.face->edge;
//       auto is_inside = box.contains(half_edge->start->position);
//       auto is_edge_dirty = !is_inside;
//       auto* incoming_half_edge =  static_cast<demo::half_edge*>(nullptr);
//       auto* outgoing_half_edge = static_cast<demo::half_edge*>(nullptr);
//       auto incoming_side = box::side{}; 
//       auto outgoing_side = box::side{};
  
//       do {
//         const auto intersections = box.intersections(half_edge->start->position, half_edge->end->position);
//         auto next_inside = box.contains(half_edge->end->position);
//         auto* next_half_edge = half_edge->next;

//         if (!is_inside && !next_inside) {
//           if (intersections.size() == 0u) {
//             points_to_remove.emplace(half_edge->start);
//             _remove_half_edge(half_edge);
//           } else if (intersections.size() == 2u) {
//             points_to_remove.emplace(half_edge->start);

//             if (processed_half_edges.find(half_edge->twin) != processed_half_edges.end()) {
//               half_edge->start = half_edge->twin->end;
//               half_edge->end = half_edge->twin->start;
//             } else {
//               half_edge->start = _create_point(intersections[0].position);
//               half_edge->end = _create_point(intersections[1].position);
//             }

//             if (outgoing_half_edge != nullptr) {
//               _link(box, outgoing_half_edge, outgoing_side, half_edge, intersections[0].side);
//             }

//             if (incoming_half_edge == nullptr) {
//               incoming_half_edge = half_edge;
//               incoming_side = intersections[0].side;
//             }

//             outgoing_half_edge = half_edge;
//             outgoing_side = intersections[1].side;
//             processed_half_edges.emplace(half_edge);
//           } else {
//             error = true;
//           }
//         } else if (is_inside && !next_inside) {
//           if (intersections.size() == 1u) {
//             if (processed_half_edges.find(half_edge->twin) != processed_half_edges.end()) {
//               half_edge->end = half_edge->twin->start;
//             } else {
//               half_edge->end = _create_point(intersections[0].position);
//             }

//             outgoing_half_edge = half_edge;
//             outgoing_side = intersections[0].side;
//             processed_half_edges.emplace(half_edge);
//           } else {
//             error = true;
//           }
//         } else if (!is_inside && next_inside) {
//           if (intersections.size() == 1u) {
//             points_to_remove.emplace(half_edge->start);
//             if (processed_half_edges.find(half_edge->twin) != processed_half_edges.end()) {
//               half_edge->start = half_edge->twin->end;
//             } else {
//               half_edge->start = _create_point(intersections[0].position);
//             }

//             if (outgoing_half_edge != nullptr) {
//               _link(box, outgoing_half_edge, outgoing_side, half_edge, intersections[0].side);
//             }

//             if (incoming_half_edge == nullptr) {
//               incoming_half_edge = half_edge;
//               incoming_side = intersections[0].side;
//             }

//             processed_half_edges.emplace(half_edge);
//           } else {
//             error = true;
//           }
//         }

//         half_edge = next_half_edge;
//         is_inside = next_inside;
//       } while (half_edge != site.face->edge);

//       if (is_edge_dirty && incoming_half_edge != nullptr) {
//         _link(box, outgoing_half_edge, outgoing_side, incoming_half_edge, incoming_side);
//       }

//       if (is_edge_dirty) {
//         site.face->edge = incoming_half_edge;
//       }
//     }

//     for (auto* vertex : points_to_remove) {
//       _remove_point(vertex);
//     }

//     return !error;
//   }

// private:

//   std::vector<site> _sites;
//   std::vector<face> _faces;
//   std::list<point> _points;
//   std::list<half_edge> _half_edges;

//   auto _create_point(const sbx::math::vector2& position) -> point*{
//     _points.emplace_back();
//     _points.back().position = position;
//     _points.back().iterator = std::prev(_points.end());

//     return &_points.back();
//   }

//   auto _create_corner(const box& box, const box::side side) -> point* {
//     switch (side) {
//       case box::side::left: {
//         return _create_point(sbx::math::vector2{box.left, box.top});
//       }
//       case box::side::bottom: {
//         return _create_point(sbx::math::vector2{box.left, box.bottom});
//       }
//       case box::side::right: {
//         return _create_point(sbx::math::vector2{box.right, box.bottom});
//       }
//       case box::side::top: {
//         return _create_point(sbx::math::vector2{box.right, box.top});
//       }
//       default: {
//         return nullptr;
//       }
//     }
//   }

//   auto _create_half_edge(face* face) -> half_edge* {
//     _half_edges.emplace_back();
//     _half_edges.back().face = face;
//     _half_edges.back().iterator = std::prev(_half_edges.end());

//     if (face->edge == nullptr) {
//       face->edge = &_half_edges.back();
//     }

//     return &_half_edges.back();
//   }

//   auto _link(const box& box, half_edge* start, const box::side start_side, half_edge* end, const box::side end_side) -> void {
//     auto* half_edge = start;
//     auto side = sbx::utility::to_underlying(start_side);

//     while (side != sbx::utility::to_underlying(end_side)) {
//       side = sbx::utility::fast_mod(side + 1u, 4u);
//       half_edge->next = _create_half_edge(start->face);
//       half_edge->next->previous = half_edge;
//       half_edge->next->start = half_edge->end;
//       half_edge->next->end = _create_corner(box, sbx::utility::from_underlying<box::side>(side));
//       half_edge = half_edge->next;
//     }

//     half_edge->next = _create_half_edge(start->face);
//     half_edge->next->previous = half_edge;
//     end->previous = half_edge->next;
//     half_edge->next->next = end;
//     half_edge->next->start = half_edge->end;
//     half_edge->next->end = end->start;
//   }

//   auto _remove_point(point* point) -> void {
//     _points.erase(point->iterator);
//   }

//   auto _remove_half_edge(half_edge* half_edge) -> void {
//     _half_edges.erase(half_edge->iterator);
//   }

// }; // class voronoi_diagram

// class fortune_algorithm {

// public:

//   fortune_algorithm(const std::vector<sbx::math::vector2>& points)
//   : _diagram{std::make_unique<voronoi_diagram>(points)} { }

//   ~fortune_algorithm() {

//   }

//   auto construct() -> void {
//     for (auto& site : _diagram->sites()) {
//       _events.push(std::make_unique<event>(site_event{&site}));
//     }

//     while (!_events.is_empty()) {
//       auto event = _events.pop();

//       _y_position = event->y();

//       if (event->is_site_event()) {
//         _handle_site_event(event->site_event());
//       } else {
//         _handle_circle_event(event->circle_event());
//       }
//     }
//   }

//   auto bound(box box) -> bool {
//     for (const auto& point : _diagram->points()) {
//       box.left = std::min(point.position.x(), box.left);
//       box.bottom = std::min(point.position.y(), box.bottom);
//       box.right = std::max(point.position.x(), box.right);
//       box.top = std::max(point.position.y(), box.top);
//     }

//     auto linked_points = std::list<linked_vertex>{};
//     auto points = std::unordered_map<std::size_t, std::array<linked_vertex*, 8u>>{_diagram->sites().size()};

//     if (!_beach_line.is_empty()) {
//       auto* left_arc = _beach_line.left_most_arc();
//       auto* right_arc = left_arc->next;
//       while (!_beach_line.is_nil(right_arc)) {
//         auto direction = sbx::math::vector2::orthogonal(left_arc->site->position - right_arc->site->position);
//         auto origin = (left_arc->site->position + right_arc->site->position) * 0.5f;

//         auto intersection = box.first_intersection(origin, direction);

//         auto* point = _diagram->_create_point(intersection.position);
//         _set_destination(left_arc, right_arc, point);

//         if (points.find(left_arc->site->index) == points.end()) {
//           points[left_arc->site->index].fill(nullptr); 
//         }

//         if (points.find(right_arc->site->index) == points.end()) {
//           points[right_arc->site->index].fill(nullptr); 
//         }

//         linked_points.emplace_back(linked_vertex{nullptr, point, left_arc->right_half_edge});
//         points[left_arc->site->index][2 * static_cast<int>(intersection.side) + 1] = &linked_points.back();
//         linked_points.emplace_back(linked_vertex{right_arc->left_half_edge, point, nullptr});
//         points[right_arc->site->index][2 * static_cast<int>(intersection.side)] = &linked_points.back();

//         left_arc = right_arc;
//         right_arc = right_arc->next;
//       }
//     }

//     for (auto& [key, cell_vertices] : points) {
//       for (auto i : std::views::iota(0u, 5u)) {
//         auto side = sbx::utility::fast_mod(i, 4u);
//         auto next_side = (side + 1) % 4;

//         if (cell_vertices[2 * side] == nullptr && cell_vertices[2 * side + 1] != nullptr) {
//           auto previous_side = (side + 3) % 4;
//           auto* corner = _diagram->_create_corner(box, sbx::utility::from_underlying<box::side>(side));

//           linked_points.emplace_back(linked_vertex{nullptr, corner, nullptr});
//           cell_vertices[2 * previous_side + 1] = &linked_points.back();
//           cell_vertices[2 * side] = &linked_points.back();
//         } else if (cell_vertices[2 * side] != nullptr && cell_vertices[2 * side + 1] == nullptr) {
//           auto* corner = _diagram->_create_corner(box, sbx::utility::from_underlying<box::side>(next_side));
//           linked_points.emplace_back(linked_vertex{nullptr, corner, nullptr});
//           cell_vertices[2 * side + 1] = &linked_points.back();
//           cell_vertices[2 * next_side] = &linked_points.back();
//         }
//       }
//     }

//     for (auto& [i, cell_vertices] : points) {
//       for (std::size_t side = 0; side < 4; ++side) {
//         if (cell_vertices[2 * side] != nullptr) {
//           auto* half_edge = _diagram->_create_half_edge(&_diagram->faces().at(i));
//           half_edge->start = cell_vertices[2 * side]->position;
//           half_edge->end = cell_vertices[2 * side + 1]->position;
//           cell_vertices[2 * side]->next_half_edge = half_edge;
//           half_edge->previous = cell_vertices[2 * side]->previous_half_edge;

//           if (cell_vertices[2 * side]->previous_half_edge != nullptr) {
//             cell_vertices[2 * side]->previous_half_edge->next = half_edge;
//           }

//           cell_vertices[2 * side + 1]->previous_half_edge = half_edge;
//           half_edge->next = cell_vertices[2 * side + 1]->next_half_edge;

//           if (cell_vertices[2 * side + 1]->next_half_edge != nullptr) {
//             cell_vertices[2 * side + 1]->next_half_edge->previous = half_edge;
//           }
//         }
//       }
//     }

//     return true;
//   }

//   auto lloyd_relexation(const std::size_t iteration) -> void {
//     for (auto i : std::views::iota(0u, iteration)) {
//       auto new_points = std::vector<sbx::math::vector2>{};

//       for (const auto& site : _diagram->sites()) {
//         auto* half_edge = site.face->edge;
//         auto* current_half_edge = half_edge;
//         auto area = 0.0f;
//         auto centroid = sbx::math::vector2{0.0f, 0.0f};

//         do {
//           auto determinant = sbx::math::vector2::determinant(current_half_edge->start->position, current_half_edge->end->position);
//           area += determinant;
//           centroid += (current_half_edge->start->position + current_half_edge->end->position) * determinant;
//           current_half_edge = current_half_edge->next;
//         } while (current_half_edge != half_edge);

//         area *= 0.5f;
//         centroid /= 6.0f * area;
//         new_points.push_back(centroid);
//       }

//       _diagram = std::make_unique<voronoi_diagram>(new_points);
//       construct();
//     }
//   }

//   auto diagram() const -> const voronoi_diagram& {
//     return *_diagram;
//   }

// private:

//   struct linked_vertex {
//     half_edge* previous_half_edge;
//     point* position;
//     half_edge* next_half_edge;
//   }; // struct linked_vertex

//   auto _handle_site_event(site_event& event) -> void {
//     auto site = event.site;

//     // 1. Check if the bach_line is empty
//     if (_beach_line.is_empty()) {
//       _beach_line.set_root(_beach_line.create_arc(site));
//       return;
//     }

//     // 2. Look for the arc above the site
//     auto* arc_to_break = _beach_line.arc_above(site->position, _y_position);
//     _delete_event(arc_to_break);

//     // 3. Replace this arc by the new arcs
//     auto* middle_arc = _break_arc(arc_to_break, site);
//     auto* left_arc = middle_arc->previous; 
//     auto* right_arc = middle_arc->next;

//     // 4. Add an edge in the diagram
//     _add_edge(left_arc, middle_arc);
//     middle_arc->right_half_edge = middle_arc->left_half_edge;
//     right_arc->left_half_edge = left_arc->right_half_edge;

//     // 5. Check circle events
//     // Left triplet
//     if (!_beach_line.is_nil(left_arc->previous)) {
//       _add_event(left_arc->previous, left_arc, middle_arc);
//     }
//     // Right triplet
//     if (!_beach_line.is_nil(right_arc->next)) {
//       _add_event(middle_arc, right_arc, right_arc->next);
//     }
//   }

//   auto _handle_circle_event(circle_event& event) -> void {
//     auto center = event.center;
//     auto* arc = event.arc;

//     // 1. Add vertex
//     auto* position = _diagram->_create_point(center);

//     // 2. Delete all the events with this arc
//     auto* left_arc = arc->previous;
//     auto* right_arc = arc->next;
//     _delete_event(left_arc);
//     _delete_event(right_arc);

//     // 3. Update the beach_line and the diagram
//     _remove_arc(arc, position);

//     // 4. Add new circle events
//     // Left triplet
//     if (!_beach_line.is_nil(left_arc->previous)) {
//       _add_event(left_arc->previous, left_arc, right_arc);
//     }
//     // Right triplet
//     if (!_beach_line.is_nil(right_arc->next)) {
//       _add_event(left_arc, right_arc, right_arc->next);
//     }
//   }

//   auto _break_arc(demo::arc* arc, demo::site* site) -> demo::arc* {
//     // Create the new subtree
//     auto* middle_arc = _beach_line.create_arc(site);
//     auto* left_arc = _beach_line.create_arc(arc->site);
//     left_arc->left_half_edge = arc->left_half_edge;
//     auto* right_arc = _beach_line.create_arc(arc->site);
//     right_arc->right_half_edge = arc->right_half_edge;

//     // Insert the subtree in the beach_line
//     _beach_line.replace(arc, middle_arc);
//     _beach_line.insert_before(middle_arc, left_arc);
//     _beach_line.insert_after(middle_arc, right_arc);

//     // Delete old arc
//     delete arc;

//     // Return the middle arc
//     return middle_arc;
//   }

//   auto _remove_arc(arc* arc, point* position) -> void {
//     _set_destination(arc->previous, arc, position);
//     _set_destination(arc, arc->next, position);

//     arc->left_half_edge->next = arc->right_half_edge;
//     arc->right_half_edge->previous = arc->left_half_edge;

//     _beach_line.remove(arc);

//     auto previous_half_edge = arc->previous->right_half_edge;
//     auto next_half_edge = arc->next->left_half_edge;

//     _add_edge(arc->previous, arc->next);
//     _set_origin(arc->previous, arc->next, position);
//     _set_previous_half_edge(arc->previous->right_half_edge, previous_half_edge);
//     _set_previous_half_edge(next_half_edge, arc->next->left_half_edge);

//     delete arc;
//   }

//   auto _is_moving_right(const arc* left, const arc* right) -> bool {
//     return left->site->position.y() < right->site->position.y();
//   }

//   auto _initial_x(const arc* left, const arc* right, const bool is_moving_right) -> std::float_t {
//     return is_moving_right ? left->site->position.x() : right->site->position.x();
//   }

//   auto _add_edge(arc* left, arc* right) -> void {
//     left->right_half_edge = _diagram->_create_half_edge(left->site->face);
//     right->left_half_edge = _diagram->_create_half_edge(right->site->face);
//     left->right_half_edge->twin = right->left_half_edge;
//     right->left_half_edge->twin = left->right_half_edge;
//   }

//   auto _set_origin(arc* left, arc* right, point* position) -> void {
//     left->right_half_edge->end = position;
//     right->left_half_edge->start = position;
//   }

//   auto _set_destination(arc* left, arc* right, point* position) -> void {
//     left->right_half_edge->start = position;
//     right->left_half_edge->end = position;
//   }

//   auto _set_previous_half_edge(half_edge* previous, half_edge* next) -> void {
//     previous->next = next;
//     next->previous = previous;
//   }

//   auto _add_event(arc* left, arc* middle, arc* right) -> void {
//     auto y = std::float_t{};
//     auto convergence_point = _compute_convergence_point(left->site->position, middle->site->position, right->site->position, y);

//     auto is_below = y <= _y_position;
//     auto left_breakpoint_moving_right = _is_moving_right(left, middle);
//     auto right_breakpoint_moving_right = _is_moving_right(middle, right);
//     auto left_initial_x = _initial_x(left, middle, left_breakpoint_moving_right);
//     auto right_initial_x = _initial_x(middle, right, right_breakpoint_moving_right);
//     auto is_valid =
//       ((left_breakpoint_moving_right && left_initial_x < convergence_point.x()) ||
//       (!left_breakpoint_moving_right && left_initial_x > convergence_point.x())) &&
//       ((right_breakpoint_moving_right && right_initial_x < convergence_point.x()) ||
//       (!right_breakpoint_moving_right && right_initial_x > convergence_point.x()));

//     if (is_valid && is_below) {
//       auto event = std::make_unique<demo::event>(demo::circle_event{y, convergence_point, middle});
//       middle->event = event.get();
//       _events.push(std::move(event));
//     }
//   }

//   auto _delete_event(arc* arc) -> void {
//     if (arc->event != nullptr) {
//       _events.remove(arc->event->index());
//       arc->event = nullptr;
//     }
//   }

//   auto _compute_convergence_point(const sbx::math::vector2& point1, const sbx::math::vector2& point2, const sbx::math::vector2& point3, std::float_t& y) -> sbx::math::vector2 {
//     auto v1 = sbx::math::vector2::orthogonal(point1 - point2);
//     auto v2 = sbx::math::vector2::orthogonal(point2 - point3);

//     auto delta = 0.5 * (point3 - point1);

//     auto t = sbx::math::vector2::determinant(delta, v2) / sbx::math::vector2::determinant(v1, v2);
//     auto center = 0.5 * (point1 + point2) + t * v1;
//     auto r = sbx::math::vector2::distance(center, point1);

//     y = center.y() - r;

//     return center;
//   }

//   std::unique_ptr<voronoi_diagram> _diagram;
//   beach_line _beach_line;
//   priority_queue<event> _events;
//   std::float_t _y_position;

// }; // class fortune_algorithm

// // https://pvigier.github.io/2018/11/18/fortune-algorithm-details.html

// } // namespace demo

// #endif // DEMO_TERRAIN_VORONOI_HPP_
