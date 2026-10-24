#include "sl.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    int failures = 0;

    void check(bool ok, const char *what)
    {
        if (!ok)
        {
            std::fprintf(stderr, "FAIL: %s\n", what);
            ++failures;
        }
    }

    bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }
}

int main()
{
    // Paths, creation and typed values.
    {
        sl::PropertyTree tree;
        check(!tree.exists("/ship/fuel"), "absent before use");
        tree.set_double("/ship/fuel", 12.5);
        check(tree.exists("ship/fuel"), "leading slash optional");
        check(near(tree.get_double("/ship/fuel"), 12.5), "double round trip");
        check(tree.get_int("/ship/fuel") == 13, "double read as int rounds");
        check(tree.get_string("/ship/fuel") == "12.5", "double read as string");
        tree.set_int("/ship/fuel", 3);
        check(tree.find("/ship/fuel")->type() == sl::PropertyNode::Type::real && near(tree.get_double("/ship/fuel"), 3.0), "type is sticky");
        tree.set_bool("/ship/lights", true);
        check(tree.get_bool("/ship/lights") && tree.get_int("/ship/lights") == 1, "bool");
        tree.set_string("/ship/name", "Atlantis");
        check(tree.get_string("/ship/name") == "Atlantis", "string");
        check(tree.get_double("/nothing", 7.0) == 7.0, "fallback");
        check(tree.node("/bad path/") != nullptr || true, "spaces tolerated or rejected without crashing");
        check(tree.node("/9bad") == nullptr, "invalid name rejected");
        check(tree.find("/ship")->path() == "/ship" && tree.find("/ship/fuel")->path() == "/ship/fuel", "paths");
    }

    // Indexed children.
    {
        sl::PropertyTree tree;
        tree.set_double("/engines/engine[0]/thrust", 1.0);
        tree.set_double("/engines/engine[2]/thrust", 3.0);
        tree.set_double("/engines/engine[1]/thrust", 2.0);
        const sl::PropertyNode *engines = tree.find("/engines");
        check(engines->child_count("engine") == 3, "three engines");
        const std::vector<sl::PropertyNode *> list = engines->children("engine");
        check(list.size() == 3 && list[0]->index() == 0 && list[2]->index() == 2, "ordered by index");
        check(near(tree.get_double("/engines/engine[2]/thrust"), 3.0), "indexed read");
        check(tree.find("/engines/engine[2]")->path() == "/engines/engine[2]", "indexed path");
        check(tree.find("/engines/engine[7]") == nullptr, "missing index");
        check(tree.node("/engines/engine[x]") == nullptr, "bad index rejected");
    }

    // Listeners.
    {
        sl::PropertyTree tree;
        int own = 0, subtree = 0;
        std::string last;
        sl::PropertyNode *throttle = tree.node("/ship/engine/throttle");
        throttle->set_double(0.0);
        throttle->add_listener([&](sl::PropertyNode &n) { ++own; last = n.path(); });
        const auto id = tree.node("/ship")->add_subtree_listener([&](sl::PropertyNode &) { ++subtree; });
        throttle->set_double(0.5);
        check(own == 1 && subtree == 1 && last == "/ship/engine/throttle", "listeners fire");
        throttle->set_double(0.5);
        check(own == 1, "no change, no event");
        tree.set_double("/ship/engine/throttle", 1.0);
        check(own == 2 && subtree == 2, "set by path fires");
        tree.node("/ship")->remove_listener(id);
        throttle->set_double(0.1);
        check(subtree == 2 && own == 3, "listener removed");
        // A listener that edits the tree must not corrupt iteration.
        tree.node("/a")->add_listener([&](sl::PropertyNode &) { tree.set_int("/b", 1); });
        tree.set_int("/a", 1);
        check(tree.get_int("/b") == 1, "reentrant set");
    }

    // Tied properties.
    {
        sl::PropertyTree tree;
        double altitude = 100.0;
        int stage = 1;
        bool ro_calls = false;
        tree.node("/ship/altitude")->tie(&altitude);
        tree.node("/ship/stage")->tie(&stage);
        tree.node("/ship/mass")->tie_double([&] { ro_calls = true; return 4200.0; });
        check(near(tree.get_double("/ship/altitude"), 100.0), "tied read");
        altitude = 250.0;
        check(near(tree.get_double("/ship/altitude"), 250.0), "tied follows variable");
        tree.set_double("/ship/altitude", 300.0);
        check(near(altitude, 300.0), "tied write reaches variable");
        tree.set_double("/ship/stage", 2.0);
        check(stage == 2, "tied int");
        tree.set_double("/ship/mass", 1.0);
        check(near(tree.get_double("/ship/mass"), 4200.0) && ro_calls && tree.find("/ship/mass")->is_read_only(), "read-only tie");
        int fired = 0;
        tree.node("/ship/altitude")->add_listener([&](sl::PropertyNode &) { ++fired; });
        tree.set_double("/ship/altitude", 5.0);
        check(fired == 1, "tie fires listeners on write");
        altitude = 9.0;
        tree.node("/ship/altitude")->notify();
        check(fired == 2, "notify");
        tree.node("/ship/altitude")->untie();
        altitude = 1.0;
        check(near(tree.get_double("/ship/altitude"), 9.0) && !tree.find("/ship/altitude")->is_tied(), "untie keeps value");
    }

    // Aliases.
    {
        sl::PropertyTree tree;
        tree.set_double("/engines/engine[0]/throttle", 0.25);
        sl::PropertyNode *alias = tree.node("/controls/throttle");
        check(alias->set_alias(tree.find("/engines/engine[0]/throttle")), "alias set");
        check(near(tree.get_double("/controls/throttle"), 0.25), "alias reads target");
        int fired = 0;
        tree.find("/engines/engine[0]/throttle")->add_listener([&](sl::PropertyNode &) { ++fired; });
        tree.set_double("/controls/throttle", 0.75);
        check(near(tree.get_double("/engines/engine[0]/throttle"), 0.75) && fired == 1, "alias writes target");
        check(!tree.find("/engines/engine[0]/throttle")->set_alias(alias), "alias cycle rejected");
    }

    // Removal and from-string parsing.
    {
        sl::PropertyTree tree;
        tree.set_int("/a/b/c", 1);
        check(tree.find("/a")->remove_child("b") && !tree.exists("/a/b/c"), "remove subtree");
        sl::PropertyNode *n = tree.node("/x");
        n->set_from_string("42");
        check(n->type() == sl::PropertyNode::Type::integer && n->get_int() == 42, "guess int");
        n->set_from_string("2.5");
        check(n->get_int() == 3, "typed parse keeps type");
        sl::PropertyNode *m = tree.node("/y");
        m->set_from_string("2.5");
        check(m->type() == sl::PropertyNode::Type::real, "guess real");
        sl::PropertyNode *t = tree.node("/z");
        t->set_from_string("true");
        check(t->type() == sl::PropertyNode::Type::boolean && t->get_bool(), "guess bool");
        sl::PropertyNode *s = tree.node("/w");
        s->set_from_string("hello");
        check(s->type() == sl::PropertyNode::Type::string, "guess string");
    }

    // JSON round trip, including indexed siblings.
    {
        sl::PropertyTree tree;
        tree.set_double("/ship/fuel", 1000.5);
        tree.set_string("/ship/name", "Kestrel");
        tree.set_bool("/ship/gear", true);
        tree.set_int("/ship/engines/engine[0]/id", 10);
        tree.set_int("/ship/engines/engine[1]/id", 11);
        const nlohmann::json json = tree.to_json();
        check(json["ship"]["engines"]["engine"].is_array() && json["ship"]["engines"]["engine"].size() == 2, "siblings become array");
        check(json["ship"]["fuel"] == 1000.5 && json["ship"]["gear"] == true, "json values");

        sl::PropertyTree copy;
        check(copy.from_json(json), "from_json");
        check(near(copy.get_double("/ship/fuel"), 1000.5) && copy.get_string("/ship/name") == "Kestrel", "round trip values");
        check(copy.get_int("/ship/engines/engine[1]/id") == 11 && copy.find("/ship/engines")->child_count("engine") == 2, "round trip indexes");

        const std::string file = "/tmp/proptree_test.json";
        check(tree.save_json(file, "/ship"), "save_json");
        sl::PropertyTree loaded;
        check(loaded.load_json(file, "/loaded"), "load_json");
        check(near(loaded.get_double("/loaded/fuel"), 1000.5), "load under a path");
        check(!loaded.load_json("/tmp/does-not-exist.json"), "missing file fails");

        // Loading into a tied node drives the C++ state.
        double fuel = 0.0;
        sl::PropertyTree tied;
        tied.node("/ship/fuel")->tie(&fuel);
        tied.from_json(json);
        check(near(fuel, 1000.5), "load drives tied variable");
        tied.node("/ship/readonly")->tie_double([] { return 1.0; });
        check(!tied.to_json()["ship"].contains("readonly") && tied.to_json("/", true)["ship"].contains("readonly"), "read-only ties skipped unless requested");
    }

    // Lua binding.
    {
        sl::PropertyTree tree;
        double throttle = 0.0;
        tree.node("/controls/throttle")->tie(&throttle);
        tree.set_double("/ship/fuel", 500.0);
        tree.set_string("/ship/name", "Kestrel");
        tree.set_int("/engines/engine[0]/id", 1);
        tree.set_int("/engines/engine[1]/id", 2);
        {
            sl::LuaRuntime lua;
            lua.initialise();
            sl::bind_property_tree(lua, tree);
            auto run = [&](const char *script) {
                const sl::LuaScriptResult result = lua.execute(script);
                if (!result.success)
                    std::fprintf(stderr, "lua error: %s\n", result.error.c_str());
                return result.success;
            };
            check(run("assert(props.get('/ship/fuel') == 500.0) assert(props.get('/ship/name') == 'Kestrel') assert(props.get('/nope') == nil)"), "lua get");
            check(run("props.set('/controls/throttle', 0.8) props.set('/ship/stage', 3) props.set('/ship/lights', true)"), "lua set");
            check(near(throttle, 0.8), "lua write reaches C++ variable");
            check(tree.get_int("/ship/stage") == 3 && tree.get_bool("/ship/lights"), "lua created nodes with types");
            check(tree.find("/ship/stage")->type() == sl::PropertyNode::Type::integer, "lua integer stays integer");
            check(run("assert(props.exists('/ship/fuel')) assert(not props.exists('/zzz'))"), "lua exists");
            check(run("local c = props.children('/engines') assert(#c == 2 and c[1] == 'engine' and c[2] == 'engine[1]')"), "lua children");
            check(run("local n = props.node('/ship/fuel') assert(n:value() == 500.0) n:set(250) assert(n:path() == '/ship/fuel' and n:name() == 'fuel')"), "lua node");
            check(near(tree.get_double("/ship/fuel"), 250.0), "lua node set");
            check(run("local e = props.node('/engines') assert(e:child('engine', 1):child('id'):value() == 2)"), "lua child");
            check(run("hits = 0 last = '' id = props.listen('/ship/fuel', function(p, v) hits = hits + 1 last = p .. '=' .. tostring(v) end)"), "lua listen");
            tree.set_double("/ship/fuel", 100.0);
            check(run("assert(hits == 1 and last == '/ship/fuel=100.0', last)"), "lua listener called from C++");
            check(run("props.set('/ship/fuel', 90) assert(hits == 2)"), "lua listener called from Lua");
            check(run("props.unlisten(id)"), "lua unlisten");
            tree.set_double("/ship/fuel", 80.0);
            check(run("assert(hits == 2)"), "listener removed");
            check(run("props.node('/ship/name'):on_change(function() end)"), "node on_change");
        }
        // The runtime is gone; changing the property must not call into it.
        tree.set_string("/ship/name", "Other");
        check(tree.get_string("/ship/name") == "Other", "listeners cleaned up when runtime closes");
    }

    if (failures == 0)
        std::puts("proptree_test: all checks passed");
    return failures == 0 ? 0 : 1;
}
