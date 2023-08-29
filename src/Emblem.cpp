#include "Emblem.h"
#include "BinaryWriter.h"
#include "json.h"

namespace {

    enum class GeometrizeShapeType {
        Rectangle        = 0,
        RotatedRectangle = 1,
        RotatedEllipse   = 4,
    };

    struct BoundingBox {
        int minX = std::numeric_limits<int>::max();
        int minY = std::numeric_limits<int>::max();
        int maxX = std::numeric_limits<int>::min();
        int maxY = std::numeric_limits<int>::min();
    };

    BoundingBox calcBoundingBox(const nlohmann::json& json) {
        BoundingBox bb;

        bb.minX = json[0]["data"][0];
        bb.minY = json[0]["data"][1];
        bb.maxX = json[0]["data"][2];
        bb.maxY = json[0]["data"][3];

        return bb;

        // Important:
        //  Since we currently use the geometrize website as a json source, we will always get a rectable that represents
        //  the bounding box in the first position of the json. This might not stay this way forever, specifically if proper
        //  alpha backgrounds are ever implemented. Keeping the code below commented out instead of deleting since it might become useful if the bb situation changes.

        // for(const auto& shape : json) {
        //     int px{};
        //     int py{};
        //     switch(shape["type"].get<GeometrizeShapeType>()) {
        //     case GeometrizeShapeType::Rectangle: {
        //         auto minX = shape["data"][0].get<float>();
        //         auto minY = shape["data"][1].get<float>();
        //         auto maxX = shape["data"][2].get<float>();
        //         auto maxY = shape["data"][3].get<float>();

        //        px = (maxX - minX);
        //        py = (maxY - minY);

        //    } break;
        //    case GeometrizeShapeType::RotatedEllipse:
        //        px = shape["data"][0];
        //        py = shape["data"][1];
        //        break;
        //    }
        //    bb.minX = px < bb.minX ? px : bb.minX;
        //    bb.maxX = px > bb.maxX ? px : bb.maxX;
        //    bb.minY = py < bb.minY ? py : bb.minY;
        //    bb.maxY = py > bb.maxY ? py : bb.maxY;
        //}
        //
        // return bb;
    }

} // namespace

ErrorOr<Emblem> Emblem::fromJson(const nlohmann::json& json) {
    Emblem emblem;

    auto bb      = calcBoundingBox(json);
    float scaleX = 256.f / (bb.maxX - bb.minX);
    float scaleY = 256.f / (bb.maxY - bb.minY);

    for(const auto& shape : json) {
        LayerDesc layerDesc;

        layerDesc.rgba.r = shape["color"][0];
        layerDesc.rgba.g = shape["color"][1];
        layerDesc.rgba.b = shape["color"][2];
        layerDesc.rgba.a = 100.f * shape["color"][3].get<float>() / 255.f;

        switch(shape["type"].get<GeometrizeShapeType>()) {
        case GeometrizeShapeType::Rectangle:
        case GeometrizeShapeType::RotatedRectangle: {
            layerDesc.decalId = DecalType::SquareSolid;
            layerDesc.angle   = 0;

            auto minX = (shape["data"][0].get<float>() * scaleX - 128) * 0x10;
            auto minY = (shape["data"][1].get<float>() * scaleY - 128) * 0x10;
            auto maxX = (shape["data"][2].get<float>() * scaleX - 128) * 0x10;
            auto maxY = (shape["data"][3].get<float>() * scaleY - 128) * 0x10;

            layerDesc.posX   = std::midpoint(minX, maxX);
            layerDesc.posY   = std::midpoint(minY, maxY);
            layerDesc.scaleX = (maxX - minX);
            layerDesc.scaleY = (maxY - minY);

            if(shape["type"].get<GeometrizeShapeType>() == GeometrizeShapeType::RotatedRectangle)
                layerDesc.angle = shape["data"][4];

        } break;
        case GeometrizeShapeType::RotatedEllipse:
            layerDesc.decalId = DecalType::EllipseSolid;
            layerDesc.posX    = (shape["data"][0] * scaleX - 128) * 0x10;
            layerDesc.posY    = (shape["data"][1] * scaleY - 128) * 0x10;
            layerDesc.scaleX  = shape["data"][2] * scaleX * 0x10 * 2;
            layerDesc.scaleY  = shape["data"][3] * scaleY * 0x10 * 2;
            layerDesc.angle   = shape["data"][4];
            break;
        default:
            return Error{ "Invalid GeometrizeShapeType: {}\n"
                          "This tool supports only Rectangles, rotated Rectangles and rotated ellipses",
                          (int)shape["type"].get<GeometrizeShapeType>() };
        }

        emblem.layers.push_back(layerDesc);
    }
    return emblem;
}

class EmbcBuilder {
private:
    struct BlockHeader {
        char name[0x10]{};
        uint64_t dataSize{};
        uint64_t _paddding{};
    };

public:
    EmbcBuilder() {
        addBlock("---- begin ----");
    }

    void addBlock(const std::string& name) {
        assert(name.size() < 0x10);

        BlockHeader header;
        std::copy(name.begin(), name.end(), header.name);

        writer.write(header);
    }

    template <typename T>
    void addBlock(const std::string& name, const T& data) {
        static_assert(std::is_trivially_copyable_v<T>);
        static_assert(!std::is_pointer_v<T>);

        addBlock(name, reinterpret_cast<const uint8_t*>(&data), sizeof(T));
    }

    template <typename T>
    void addBlock(const std::string& name, const T* data, int64_t size) {
        static_assert(std::is_trivially_copyable_v<T>);

        BlockHeader header;
        assert(name.size() < 0x10);
        std::copy(name.begin(), name.end(), header.name);
        header.dataSize = size * sizeof(T);

        writer.write(header);
        writer.write(data, size);
    }


    std::vector<uint8_t> build() {
        addBlock("----  end  ----");
        return writer.release();
    }

private:
    SequentialBinaryWriter writer;
};

std::vector<uint8_t> Emblem::serialize() const {
    EmbcBuilder builder;

    builder.addBlock("Category", category);
    builder.addBlock("UgcID", ugcId);
    builder.addBlock("DateTime", dateTime, sizeof(dateTime));

    // TODO: This is a bit ugly since some last second changed turned up after the API was already done. Could be reworked.
    std::vector<uint8_t> layerData(layers.size() * sizeof(LayerDesc) + 4);
    short unk        = 0;
    short layerCount = layers.size();
    std::memcpy(layerData.data() + 0x00, &unk, sizeof(unk));
    std::memcpy(layerData.data() + 0x02, &layerCount, sizeof(layerCount));
    std::memcpy(layerData.data() + 0x04, layers.data(), layers.size() * sizeof(LayerDesc));
    builder.addBlock("Image", layerData.data(), layerData.size());

    return builder.build();
}