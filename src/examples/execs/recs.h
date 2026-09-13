/*
------------------------------------------------------------------------------
recs.h - Really Explicit Component System (ECS)

Usage:
    #define RECS_IMPLEMENTATION
    #include "recs.h"

Design goals:
    - Data-oriented
    - SIMD-friendly
    - Archetype-based
    - Minimal abstraction
    - Header-only (STB style)

------------------------------------------------------------------------------
*/
#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <new>

#ifndef RECS_MAX_COMPONENTS
#define RECS_MAX_COMPONENTS 64
#endif

namespace recs {

//============================================================
// Entity
//============================================================
struct Entity {
    uint32_t id;
    uint32_t generation;
};

static constexpr Entity INVALID_ENTITY{0, 0};

//============================================================
// Component ID
//============================================================
using ComponentID = uint32_t;

inline ComponentID next_component_id() {
    static ComponentID id = 0;
    assert(id < RECS_MAX_COMPONENTS && "component_id exceeded RECS_MAX_COMPONENTS");
    return id++;
}

template<typename T>
ComponentID component_id() {
    static ComponentID id = next_component_id();
    return id;
}

//============================================================
// Archetype Key (bitmask)
//============================================================
struct ArchetypeKey {
    uint64_t mask = 0;

    void add(ComponentID id) { mask |= (1ULL << id); }
    void remove(ComponentID id) { mask &= ~(1ULL << id); }
    bool has(ComponentID id) const { return mask & (1ULL << id); }

    bool operator==(const ArchetypeKey& o) const {
        return mask == o.mask;
    }
};

//============================================================
// Component Storage (SoA)
//============================================================
struct ComponentArray {
    void* data = nullptr;
    size_t stride = 0;

    void (*destroy)(void*) = nullptr;
    void* (*create)() = nullptr;
    void (*move_and_pop)(void*, size_t, size_t) = nullptr;
    void (*copy_element)(void*, void*, size_t) = nullptr;
    void (*emplace_default)(void*) = nullptr;
};

inline ComponentArray clone_empty(const ComponentArray& proto) {
    ComponentArray copy = proto;
    copy.data = proto.create ? proto.create() : nullptr;
    return copy;
}

template<typename T>
ComponentArray make_array() {
    ComponentArray arr;
    arr.data = new std::vector<T>();
    arr.stride = sizeof(T);
    arr.destroy = [](void* p) {
        delete static_cast<std::vector<T>*>(p);
    };
    arr.create = []() -> void* {
        return new std::vector<T>();
    };
    arr.move_and_pop = [](void* p, size_t index, size_t last) {
        auto vec = static_cast<std::vector<T>*>(p);
        (*vec)[index] = std::move((*vec)[last]);
        vec->pop_back();
    };
    arr.copy_element = [](void* dst, void* src, size_t src_idx) {
        auto d = static_cast<std::vector<T>*>(dst);
        auto s = static_cast<std::vector<T>*>(src);
        d->push_back((*s)[src_idx]);
    };
    arr.emplace_default = [](void* p) {
        auto vec = static_cast<std::vector<T>*>(p);
        vec->emplace_back();
    };
    return arr;
}

//============================================================
// Archetype
//============================================================
struct Archetype {
    ArchetypeKey key;
    std::vector<Entity> entities;
    std::array<ComponentArray, RECS_MAX_COMPONENTS> components{};
};

//============================================================
// World
//============================================================
class World {
public:
    World() = default;
    ~World() {
        // Clean up resources
        for (auto& [_, res] : resources_) {
            if (res.data && res.destroy) {
                res.destroy(res.data);
            }
        }
        // Clean up archetypes
        for (auto& [_, arch] : archetypes_) {
            destroy_archetype(arch);
        }
    }

    // Delete copy constructor and assignment
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Enable move semantics
    World(World&& other) noexcept
        : generations_(std::move(other.generations_))
        , free_ids_(std::move(other.free_ids_))
        , archetypes_(std::move(other.archetypes_))
        , entity_locations_(std::move(other.entity_locations_))
        , resources_(std::move(other.resources_))
        , event_handlers_(std::move(other.event_handlers_))
    {}

    World& operator=(World&& other) noexcept {
        if (this != &other) {
            // Clean up current resources
            for (auto& [_, res] : resources_) {
                if (res.data && res.destroy) {
                    res.destroy(res.data);
                }
            }
            for (auto& [_, arch] : archetypes_) {
                destroy_archetype(arch);
            }

            generations_ = std::move(other.generations_);
            free_ids_ = std::move(other.free_ids_);
            archetypes_ = std::move(other.archetypes_);
            entity_locations_ = std::move(other.entity_locations_);
            resources_ = std::move(other.resources_);
            event_handlers_ = std::move(other.event_handlers_);
        }
        return *this;
    }

    // --------------------------------------------------------
    // Entity management
    // --------------------------------------------------------
    Entity create() {
        std::lock_guard<std::mutex> lock(world_mutex_);
        return create_unsafe();
    }

    void destroy(Entity e) {
        std::lock_guard<std::mutex> lock(world_mutex_);
        destroy_unsafe(e);
    }

    bool alive(Entity e) const {
        std::lock_guard<std::mutex> lock(world_mutex_);
        return alive_unsafe(e);
    }

    // --------------------------------------------------------
    // Component add/remove
    // --------------------------------------------------------
    template<typename... Cs>
    void add(Entity e) {
        std::lock_guard<std::mutex> lock(world_mutex_);
        migrate<Cs...>(e, true);
    }

    template<typename C, typename... Args>
    void add(Entity e, Args&&... args) {
        std::lock_guard<std::mutex> lock(world_mutex_);
        migrate<C>(e, true);
        if (auto* comp = get_unsafe<C>(e)) {
            *comp = C{std::forward<Args>(args)...};
        }
    }

    template<typename... Cs>
    void remove(Entity e) {
        std::lock_guard<std::mutex> lock(world_mutex_);
        migrate<Cs...>(e, false);
    }

    // --------------------------------------------------------
    // Component access
    // --------------------------------------------------------
    template<typename C>
    C* get(Entity e) {
        std::lock_guard<std::mutex> lock(world_mutex_);
        return get_unsafe<C>(e);
    }

    template<typename C>
    const C* get(Entity e) const {
        std::lock_guard<std::mutex> lock(world_mutex_);
        return get_unsafe<C>(e);
    }

    template<typename C>
    bool has(Entity e) const {
        std::lock_guard<std::mutex> lock(world_mutex_);
        if (!alive_unsafe(e)) return false;
        auto& loc = entity_locations_[e.id];
        if (!loc.arch) return false;
        return loc.arch->key.has(component_id<C>());
    }

    // --------------------------------------------------------
    // Iteration (entity-level)
    // --------------------------------------------------------
    template<typename... Cs, typename Fn>
    void for_each(Fn&& fn) {
        ArchetypeKey required;
        (required.add(component_id<Cs>()), ...);

        for (auto& [_, arch] : archetypes_) {
            if ((arch.key.mask & required.mask) != required.mask)
                continue;

            size_t n = arch.entities.size();
            for (size_t i = 0; i < n; ++i) {
                fn(
                    (*get_array<Cs>(arch))[i]...
                );
            }
        }
    }

    template<typename... Cs, typename Fn>
    void for_each(Fn&& fn) const {
        ArchetypeKey required;
        (required.add(component_id<Cs>()), ...);

        for (auto& [_, arch] : archetypes_) {
            if ((arch.key.mask & required.mask) != required.mask)
                continue;

            size_t n = arch.entities.size();
            for (size_t i = 0; i < n; ++i) {
                fn(
                    (*get_array<Cs>(arch))[i]...
                );
            }
        }
    }

    // --------------------------------------------------------
    // Chunk iteration (SIMD friendly)
    // --------------------------------------------------------
    template<typename... Cs, typename Fn>
    void for_each_chunk(Fn&& fn) {
        ArchetypeKey required;
        (required.add(component_id<Cs>()), ...);

        for (auto& [_, arch] : archetypes_) {
            if ((arch.key.mask & required.mask) != required.mask)
                continue;

            if (arch.entities.empty())
                continue;

            fn(
                get_array<Cs>(arch)->data()...,
                arch.entities.size()
            );
        }
    }

    template<typename... Cs, typename Fn>
    void for_each_chunk(Fn&& fn) const {
        ArchetypeKey required;
        (required.add(component_id<Cs>()), ...);

        for (auto& [_, arch] : archetypes_) {
            if ((arch.key.mask & required.mask) != required.mask)
                continue;

            if (arch.entities.empty())
                continue;

            fn(
                get_array<Cs>(arch)->data()...,
                arch.entities.size()
            );
        }
    }

    // --------------------------------------------------------
    // Parallel iteration (thread-safe)
    // --------------------------------------------------------
    template<typename... Cs, typename Fn>
    void parallel_for_each(Fn&& fn) {
        // Collect matching archetypes with lock
        std::vector<std::pair<Archetype*, size_t>> work_items;
        {
            std::lock_guard<std::mutex> lock(world_mutex_);
            ArchetypeKey required;
            (required.add(component_id<Cs>()), ...);

            for (auto& [_, arch] : archetypes_) {
                if ((arch.key.mask & required.mask) == required.mask) {
                    size_t n = arch.entities.size();
                    if (n > 0) {
                        work_items.emplace_back(&arch, n);
                    }
                }
            }
        }
        // Lock released - now safe to parallelize reads

        // Parallelize across all entities in all archetypes
        for (auto& [arch, count] : work_items) {
            #pragma omp parallel for schedule(dynamic, 1000)
            for (size_t i = 0; i < count; ++i) {
                fn(
                    (*get_array<Cs>(*arch))[i]...
                );
            }
        }
    }

    template<typename... Cs, typename Fn>
    void parallel_for_each_chunk(Fn&& fn) {
        // Collect matching archetypes and subdivide into chunks
        struct ChunkWork {
            Archetype* arch;
            size_t start;
            size_t count;
        };
        std::vector<ChunkWork> chunks;
        
        {
            std::lock_guard<std::mutex> lock(world_mutex_);
            ArchetypeKey required;
            (required.add(component_id<Cs>()), ...);

            for (auto& [_, arch] : archetypes_) {
                if ((arch.key.mask & required.mask) == required.mask && !arch.entities.empty()) {
                    size_t total = arch.entities.size();
                    // Subdivide large archetypes into chunks for parallel processing
                    const size_t chunk_size = 4096;  // Tune based on cache line size
                    
                    for (size_t start = 0; start < total; start += chunk_size) {
                        size_t count = std::min(chunk_size, total - start);
                        chunks.push_back({&arch, start, count});
                    }
                }
            }
        }
        // Lock released - now safe to parallelize reads

        // Process chunks in parallel
        #pragma omp parallel for schedule(dynamic)
        for (size_t chunk_idx = 0; chunk_idx < chunks.size(); ++chunk_idx) {
            auto& work = chunks[chunk_idx];
            fn(
                (get_array<Cs>(*work.arch)->data() + work.start)...,
                work.count
            );
        }
    }

    // --------------------------------------------------------
    // Query builder
    // --------------------------------------------------------
    template<typename... Cs>
    struct Query {
        World* world;
        ArchetypeKey exclude_mask;

        Query(World* w) : world(w) {}

        template<typename... Es>
        Query& exclude() {
            (exclude_mask.add(component_id<Es>()), ...);
            return *this;
        }

        template<typename Fn>
        void each(Fn&& fn) {
            ArchetypeKey required;
            (required.add(component_id<Cs>()), ...);

            for (auto& [_, arch] : world->archetypes_) {
                if ((arch.key.mask & required.mask) != required.mask)
                    continue;
                if ((arch.key.mask & exclude_mask.mask) != 0)
                    continue;

                size_t n = arch.entities.size();
                for (size_t i = 0; i < n; ++i) {
                    fn(
                        (*world->get_array<Cs>(arch))[i]...
                    );
                }
            }
        }
    };

    template<typename... Cs>
    Query<Cs...> query() {
        return Query<Cs...>(this);
    }

    // --------------------------------------------------------
    // Batch operations
    // --------------------------------------------------------
    std::vector<Entity> create_batch(size_t count) {
        std::lock_guard<std::mutex> lock(world_mutex_);
        std::vector<Entity> entities;
        entities.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            entities.push_back(create_unsafe());
        }
        return entities;
    }

    void destroy_batch(const std::vector<Entity>& entities) {
        std::lock_guard<std::mutex> lock(world_mutex_);
        for (Entity e : entities) {
            destroy_unsafe(e);
        }
    }

    // --------------------------------------------------------
    // Resource management
    // --------------------------------------------------------
    template<typename R, typename... Args>
    void set_resource(Args&&... args) {
        std::lock_guard<std::mutex> lock(world_mutex_);
        auto id = component_id<R>();
        auto& res = resources_[id];
        if (res.data && res.destroy) {
            res.destroy(res.data);
        }
        res.data = new R{std::forward<Args>(args)...};
        res.destroy = [](void* p) { delete static_cast<R*>(p); };
    }

    template<typename R>
    R& get_resource() {
        std::lock_guard<std::mutex> lock(world_mutex_);
        auto id = component_id<R>();
        auto it = resources_.find(id);
        assert(it != resources_.end() && it->second.data && "resource not found");
        return *static_cast<R*>(it->second.data);
    }

    template<typename R>
    const R& get_resource() const {
        std::lock_guard<std::mutex> lock(world_mutex_);
        auto id = component_id<R>();
        auto it = resources_.find(id);
        assert(it != resources_.end() && it->second.data && "resource not found");
        return *static_cast<R*>(it->second.data);
    }

    template<typename R>
    bool has_resource() const {
        std::lock_guard<std::mutex> lock(world_mutex_);
        auto id = component_id<R>();
        auto it = resources_.find(id);
        return it != resources_.end() && it->second.data != nullptr;
    }

    // --------------------------------------------------------
    // Debug & introspection
    // --------------------------------------------------------
    size_t get_entity_count() const {
        return generations_.size() - free_ids_.size();
    }

    size_t get_archetype_count() const {
        return archetypes_.size();
    }

    void print_memory_usage() const {
        size_t total_component_bytes = 0;
        size_t total_entities = 0;

        for (const auto& [_, arch] : archetypes_) {
            total_entities += arch.entities.size();
            for (ComponentID id = 0; id < RECS_MAX_COMPONENTS; ++id) {
                if (!arch.key.has(id)) continue;
                const auto& comp = arch.components[id];
                if (comp.data) {
                    total_component_bytes += comp.stride * arch.entities.size();
                }
            }
        }

        std::cout << "=== ECS Memory Usage ===\n";
        std::cout << "Entities: " << total_entities << "\n";
        std::cout << "Archetypes: " << archetypes_.size() << "\n";
        std::cout << "Component data: " << (total_component_bytes / 1024.0) << " KB\n";
        std::cout << "Entity metadata: " << 
            ((generations_.size() * sizeof(uint32_t) + 
              entity_locations_.size() * sizeof(Location)) / 1024.0) << " KB\n";
    }

    // --------------------------------------------------------
    // Event system
    // --------------------------------------------------------
    template<typename C>
    void on_component_added(std::function<void(Entity)> callback) {
        auto id = component_id<C>();
        event_handlers_[id].on_add.push_back(callback);
    }

    template<typename C>
    void on_component_removed(std::function<void(Entity)> callback) {
        auto id = component_id<C>();
        event_handlers_[id].on_remove.push_back(callback);
    }

private:
    struct Location {
        Archetype* arch = nullptr;
        size_t index = 0;
    };

    struct ResourceStorage {
        void* data = nullptr;
        void (*destroy)(void*) = nullptr;
    };

    // --------------------------------------------------------
    // Internal helpers
    // --------------------------------------------------------
    
    // Unsafe versions don't acquire locks (caller must hold world_mutex_)
    bool alive_unsafe(Entity e) const {
        return e.id < generations_.size() &&
               generations_[e.id] == e.generation;
    }

    Entity create_unsafe() {
        uint32_t id;
        if (!free_ids_.empty()) {
            id = free_ids_.back();
            free_ids_.pop_back();
        } else {
            id = (uint32_t)generations_.size();
            generations_.push_back(0);
        }

        Entity e{id, generations_[id]};
        if (entity_locations_.size() <= id)
            entity_locations_.resize(id + 1);
        entity_locations_[id] = {nullptr, 0};
        return e;
    }

    void destroy_unsafe(Entity e) {
        if (!alive_unsafe(e)) return;

        auto& loc = entity_locations_[e.id];
        remove_from_archetype(e, *loc.arch, loc.index);

        generations_[e.id]++;
        free_ids_.push_back(e.id);
    }

    template<typename C>
    C* get_unsafe(Entity e) {
        if (!alive_unsafe(e)) return nullptr;
        auto& loc = entity_locations_[e.id];
        if (!loc.arch) return nullptr;
        if (!loc.arch->key.has(component_id<C>())) return nullptr;
        return &(*get_array<C>(*loc.arch))[loc.index];
    }

    template<typename C>
    const C* get_unsafe(Entity e) const {
        if (!alive_unsafe(e)) return nullptr;
        auto& loc = entity_locations_[e.id];
        if (!loc.arch) return nullptr;
        if (!loc.arch->key.has(component_id<C>())) return nullptr;
        return &(*get_array<C>(*loc.arch))[loc.index];
    }

    template<typename C>
    std::vector<C>* get_array(Archetype& arch) {
        auto& arr = arch.components[component_id<C>()];
        assert(arr.data && "component array not initialized");
        return static_cast<std::vector<C>*>(arr.data);
    }

    template<typename C>
    const std::vector<C>* get_array(const Archetype& arch) const {
        auto& arr = arch.components[component_id<C>()];
        assert(arr.data && "component array not initialized");
        return static_cast<std::vector<C>*>(arr.data);
    }

    template<typename C>
    ComponentArray& ensure_array(Archetype& arch) {
        auto id = component_id<C>();
        auto& arr = arch.components[id];
        if (!arr.data) arr = make_array<C>();
        return arr;
    }

    void remove_from_archetype(Entity e, Archetype& arch, size_t index) {
        size_t last = arch.entities.size() - 1;
        Entity moved = arch.entities[last];

        arch.entities[index] = moved;
        arch.entities.pop_back();

        for (ComponentID id = 0; id < RECS_MAX_COMPONENTS; ++id) {
            if (!arch.key.has(id)) continue;
            auto& comp = arch.components[id];
            assert(comp.move_and_pop && comp.data);
            comp.move_and_pop(comp.data, index, last);
        }

        entity_locations_[moved.id].index = index;
        entity_locations_[e.id] = {nullptr, 0};
    }

    template<typename... Cs>
    void migrate(Entity e, bool adding) {
        ArchetypeKey new_key;
        Archetype* old_arch = entity_locations_[e.id].arch;

        if (old_arch)
            new_key = old_arch->key;

        if (adding) {
            (new_key.add(component_id<Cs>()), ...);
        } else {
            (new_key.remove(component_id<Cs>()), ...);
        }

        Archetype& new_arch = get_or_create_archetype(new_key);
        size_t new_index = new_arch.entities.size();
        new_arch.entities.push_back(e);

        if (adding) {
            (ensure_array<Cs>(new_arch), ...);
        }

        if (old_arch) {
            size_t old_index = entity_locations_[e.id].index;

            for (ComponentID id = 0; id < RECS_MAX_COMPONENTS; ++id) {
                if (!new_key.has(id)) continue;

                auto& dst = new_arch.components[id];
                if (!dst.data) {
                    if (old_arch->key.has(id)) {
                        dst = clone_empty(old_arch->components[id]);
                    }
                }
            }

            for (ComponentID id = 0; id < RECS_MAX_COMPONENTS; ++id) {
                if (!new_key.has(id)) continue;
                auto& dst = new_arch.components[id];

                if (old_arch->key.has(id)) {
                    auto& src = old_arch->components[id];
                    assert(dst.copy_element && src.data);
                    dst.copy_element(dst.data, src.data, old_index);
                } else {
                    assert(dst.emplace_default && dst.data);
                    dst.emplace_default(dst.data);
                }
            }

            remove_from_archetype(e, *old_arch, old_index);
        } else {
            for (ComponentID id = 0; id < RECS_MAX_COMPONENTS; ++id) {
                if (!new_key.has(id)) continue;
                auto& comp = new_arch.components[id];
                assert(comp.emplace_default && comp.data);
                comp.emplace_default(comp.data);
            }
        }

        entity_locations_[e.id] = {&new_arch, new_index};

        // Fire event callbacks
        if (adding) {
            (fire_component_added<Cs>(e), ...);
        } else {
            (fire_component_removed<Cs>(e), ...);
        }
    }

    Archetype& get_or_create_archetype(const ArchetypeKey& key) {
        auto it = archetypes_.find(key.mask);
        if (it != archetypes_.end())
            return it->second;

        Archetype arch;
        arch.key = key;

        return archetypes_.emplace(key.mask, std::move(arch)).first->second;
    }

    void destroy_archetype(Archetype& arch) {
        for (ComponentID id = 0; id < RECS_MAX_COMPONENTS; ++id) {
            if (!arch.key.has(id)) continue;
            auto& comp = arch.components[id];
            if (comp.destroy && comp.data) {
                comp.destroy(comp.data);
                comp.data = nullptr;
            }
        }
    }

    template<typename C>
    void fire_component_added(Entity e) {
        auto id = component_id<C>();
        auto it = event_handlers_.find(id);
        if (it != event_handlers_.end()) {
            for (auto& callback : it->second.on_add) {
                callback(e);
            }
        }
    }

    template<typename C>
    void fire_component_removed(Entity e) {
        auto id = component_id<C>();
        auto it = event_handlers_.find(id);
        if (it != event_handlers_.end()) {
            for (auto& callback : it->second.on_remove) {
                callback(e);
            }
        }
    }

private:
    mutable std::mutex world_mutex_;  // Protects all World state
    
    std::vector<uint32_t> generations_;
    std::vector<uint32_t> free_ids_;

    std::unordered_map<uint64_t, Archetype> archetypes_;
    std::vector<Location> entity_locations_;
    std::unordered_map<ComponentID, ResourceStorage> resources_;

    // Event handlers
    struct EventHandlers {
        std::vector<std::function<void(Entity)>> on_add;
        std::vector<std::function<void(Entity)>> on_remove;
    };
    std::unordered_map<ComponentID, EventHandlers> event_handlers_;
};

} // namespace recs
