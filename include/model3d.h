#pragma once

#include "surface3d.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace sl
{

    enum class ModelAnimationType
    {
        rotation,
        translation,
        spin,
        blend
    };

    /** Property-driven transform or alpha blend applied to parts with matching object names. */
    struct ModelAnimation
    {
        ModelAnimationType type = ModelAnimationType::rotation;
        std::string property;
        std::vector<std::string> objects;
        glm::vec3 axis{0.0f, 1.0f, 0.0f};
        glm::vec3 center{0.0f};
        float factor = 1.0f;
        float offset = 0.0f;
        float minimum = 0.0f;
        float maximum = 1.0f;
        std::vector<glm::vec2> interpolation;
    };

    enum class ModelConditionType
    {
        property,
        less_than,
        greater_than,
        equal,
        not_equal,
        less_equal,
        greater_equal,
        logical_and,
        logical_or,
        logical_not
    };

    /** Boolean expression used by model-part visibility rules. */
    struct ModelCondition
    {
        ModelConditionType type = ModelConditionType::property;
        std::string property;
        float value = 0.0f;
        std::vector<ModelCondition> children;
    };

    /** Conditional visibility rule for named model parts. */
    struct ModelVisibility
    {
        std::vector<std::string> objects;
        ModelCondition condition;
    };

    /** One drawable piece of a Model: a mesh, its material and a transform relative to the model. */
    struct ModelPart
    {
        std::string name;
        /** Owned by the Model; may be shared by several parts. */
        Mesh *mesh = nullptr;
        /** The texture, if any, is owned by the Model. */
        Material material;
        glm::mat4 transform{1.0f};
        bool visible = true;
        /** Axis-aligned bounds of the part's geometry after `transform` (before animation); valid when has_bounds. */
        glm::vec3 bounds_min{0.0f};
        glm::vec3 bounds_max{0.0f};
        bool has_bounds = false;
    };

    /**
     * A loaded multi-part model. The model owns its meshes and textures; release it
     * with destroy_model(). Parts may be edited (for example to retint a material).
     */
    struct Model
    {
        std::vector<ModelPart> parts;
        std::vector<Mesh *> meshes;
        std::vector<Bitmap *> textures;
        std::vector<ModelAnimation> animations;
        std::vector<ModelVisibility> visibility;
        std::unordered_map<std::string, float> animation_properties;
        std::vector<std::string> unsupported_animations;
    };

    /**
     * Load a Wavefront OBJ with its .mtl materials (Kd colour and map_Kd texture; one part per
     * usemtl group). Texture paths are resolved relative to the OBJ file. Missing textures
     * are skipped. Returns nullptr on failure (see last_error()).
     */
    Model *load_obj_model(const std::string &path);

    /**
     * Load a glTF 2.0 file (.gltf with external or base64 buffers, or binary .glb). Supports
     * node hierarchies, triangle primitives, POSITION/NORMAL/TEXCOORD_0/COLOR_0, base colour
     * factor and texture, and doubleSided. Skins, animation, morph targets and sparse accessors
     * are ignored. Returns nullptr on failure.
     */
    Model *load_gltf_model(const std::string &path);

    /**
     * Load an AC3D model (.ac), preserving object transforms, names, materials, UVs and textures.
     * Polygon surfaces are triangulated; line objects are ignored. Texture paths are relative
     * to the AC3D file.
     */
    Model *load_ac3d_model(const std::string &path);

    /**
     * Load the model path and supported rotate/translate/spin/blend animations from a FlightGear
     * model XML file, including conditional visibility. Other animation types are recorded in
     * Model::unsupported_animations and not evaluated.
     */
    Model *load_flightgear_model(const std::string &path);

    /** Load simlib's JSON animation format into an already loaded model. */
    bool load_model_animations_json(Model *model, const std::string &path);
    /** Save the model's supported transform animations as simlib JSON. */
    bool save_model_animations_json(const Model *model, const std::string &path);

    /** Load by file extension: .obj, .gltf, .glb or .ac. */
    Model *load_model(const std::string &path);

    void destroy_model(Model *model);

    /** Set an input property used by the model's property-driven animations. */
    bool set_model_property(Model *model, const std::string &property, float value);

    /** True when the part's visibility flag and any imported select rules currently allow it to draw. */
    bool model_part_is_visible(const Model *model, const ModelPart &part);

    /** Draw every part into the active surface; transform is applied on top of each part's own transform. */
    bool draw_model(const Model *model, const glm::mat4 &transform);

    /**
     * Load a Wavefront OBJ file as a single mesh (v/vt/vn/f, polygons triangulated, negative
     * indices supported). Normals are generated when absent. Materials are ignored.
     */
    Mesh *load_obj_mesh(const std::string &path, Colour colour = {255, 255, 255, 255});
    /** As load_obj_mesh() but parses OBJ text already in memory. */
    Mesh *load_obj_mesh_from_memory(const char *data, std::size_t size, Colour colour = {255, 255, 255, 255});

} // namespace sl
