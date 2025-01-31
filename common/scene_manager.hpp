﻿#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "houdini_alembic.hpp"
#include "raccoon_ocl.hpp"
#include "stackless_bvh.hpp"
#include "material_type.hpp"
#include "image2d.hpp"
#include "envmap.hpp"
#include "stopwatch.hpp"
#include "timeline_profiler.hpp"

#define USE_WHITE_FURNANCE_ENV 0

namespace rt {
	struct MaterialStorage {
		std::vector<Material> materials;
		std::vector<Lambertian> lambertians;
		std::vector<Specular>   speculars;
		std::vector<Dierectric> dierectrics;
		std::vector<Ward>       wards;
		
		void add_variant(const rttr::variant &instance) {
			if (instance.is_type<std::shared_ptr<Lambertian>>()) {
				add(*instance.get_value<std::shared_ptr<Lambertian>>());
			}
			else if (instance.is_type<std::shared_ptr<Specular>>()) {
				add(*instance.get_value<std::shared_ptr<Specular>>());
			}
			else if (instance.is_type<std::shared_ptr<Dierectric>>()) {
				add(*instance.get_value<std::shared_ptr<Dierectric>>());
			}
			else if (instance.is_type<std::shared_ptr<Ward>>()) {
				add(*instance.get_value<std::shared_ptr<Ward>>());
			}
			else {
				RT_ASSERT(0);
			}
		}
		void add(const Lambertian &lambertian) {
			int index = (int)lambertians.size();
			materials.emplace_back(Material(kMaterialType_Lambertian, index));
			lambertians.emplace_back(lambertian);
		}
		void add(const Specular &specular) {
			int index = (int)speculars.size();
			materials.emplace_back(Material(kMaterialType_Specular, index));
			speculars.emplace_back(specular);
		}
		void add(const Dierectric &dierectric) {
			int index = (int)dierectrics.size();
			materials.emplace_back(Material(kMaterialType_Dierectric, index));
			dierectrics.emplace_back(dierectric);
		}
		void add(const Ward &ward) {
			int index = (int)wards.size();
			materials.emplace_back(Material(kMaterialType_Ward, index));
			wards.emplace_back(ward);
		}
	};

	// polymesh
	inline void add_materials(MaterialStorage *storage, houdini_alembic::PolygonMeshObject *p, const glm::mat3 &xformInverseTransposed) {
		SCOPED_PROFILE("add_materials()");

		storage->materials.reserve(storage->materials.size() + p->primitives.rowCount());

		auto fallback_material = []() {
			return Lambertian(glm::vec3(), glm::vec3(0.9f, 0.1f, 0.9f), false);
		};

		// fallback
		auto material_string = p->primitives.column_as_string("material");
		if (material_string == nullptr) {
			for (uint32_t i = 0, n = p->indices.size() / 3; i < n; ++i) {
				storage->add(fallback_material());
			}
			return;
		}
		
		for (uint32_t i = 0, n = p->primitives.rowCount(); i < n; ++i) {
			const std::string m = material_string->get(i);

			using namespace rttr;

			type t = type::get_by_name(m);

			// fallback
			if (t.is_valid() == false) {
				storage->add(fallback_material());
				continue;
			}
			// std::cout << t.get_name();
			variant instance = t.create();
			for (auto& prop : t.get_properties()) {
				auto meta = prop.get_metadata(kGeoScopeKey);
				RT_ASSERT(meta.is_valid());

				GeoScope scope = meta.get_value<GeoScope>();
				auto value = prop.get_value(instance);

				switch (scope)
				{
				//case rt::GeoScope::Vertices:
				//	if (value.is_type<std::array<glm::vec3, 3>>()) {
				//		if (auto v = p->vertices.column_as_vector3(prop.get_name().data())) {
				//			std::array<glm::vec3, 3> value;
				//			for (int j = 0; j < value.size(); ++j) {
				//				v->get(i * 3 + j, glm::value_ptr(value[j]));
				//			}
				//			prop.set_value(instance, value);
				//		}
				//	}
				//	break;
				case rt::GeoScope::Primitives:
					if (value.is_type<OpenCLFloat3>()) {
						if (auto v = p->primitives.column_as_vector3(prop.get_name().data())) {
							glm::vec3 value;
							v->get(i, glm::value_ptr(value));
							prop.set_value(instance, OpenCLFloat3(value));
						}
					}
					else if (value.is_type<int>()) {
						if (auto v = p->primitives.column_as_int(prop.get_name().data())) {
							prop.set_value(instance, v->get(i));
						}
					}
					else if (value.is_type<float>()) {
						if (auto v = p->primitives.column_as_float(prop.get_name().data())) {
							prop.set_value(instance, v->get(i));
						}
					}
					break;
				}
			}
			storage->add_variant(instance);
		}
	}


	struct alignas(16) TrianglePrimitive {
		uint32_t indices[3];
	};

	class SceneBuffer {
	public:
		AABB top_aabb;
		std::unique_ptr<OpenCLBuffer<StacklessBVHNode>> stacklessBVHNodesCL;
		std::unique_ptr<OpenCLBuffer<uint32_t>> primitive_idsCL;
		std::unique_ptr<OpenCLBuffer<uint32_t>> indicesCL;
		std::unique_ptr<OpenCLBuffer<OpenCLFloat3>> pointsCL;
	};

	class MaterialBuffer {
	public:
		std::unique_ptr<OpenCLBuffer<Material>> materials;
		std::unique_ptr<OpenCLBuffer<Lambertian>> lambertians;
		std::unique_ptr<OpenCLBuffer<Specular>>   speculars;
		std::unique_ptr<OpenCLBuffer<Dierectric>> dierectrics;
		std::unique_ptr<OpenCLBuffer<Ward>>       wards;
	};

	struct EnvmapFragment {
		float beg_y = 0.0f;
		float end_y = 0.0f;
		float beg_phi = 0.0f;
		float end_phi = 0.0f;
	};

	struct AliasBucket {
		float height = 0.0f;
		int alias = 0;
	};

	class EnvmapBuffer {
	public:
		std::unique_ptr<OpenCLImage> envmap;
		std::unique_ptr<OpenCLBuffer<float>> pdfs;
		std::unique_ptr<OpenCLBuffer<EnvmapFragment>> fragments;
		std::unique_ptr<OpenCLBuffer<AliasBucket>> aliasBuckets;

		std::unique_ptr<OpenCLBuffer<float>> sixAxisPdfN;
		std::unique_ptr<OpenCLBuffer<AliasBucket>> sixAxisAliasBucketN;
	};

	class SceneManager {
	public:
		SceneManager():_material_storage(new MaterialStorage()){

		}
		void setAlembicDirectory(std::filesystem::path alembicDirectory) {
			_alembicDirectory = alembicDirectory;
		}

		void addPolymesh(houdini_alembic::PolygonMeshObject *p) {
			SCOPED_PROFILE("addPolymesh()");
			SET_PROFILE_DESC(p->name.c_str());

			bool isTriangleMesh = std::all_of(p->faceCounts.begin(), p->faceCounts.end(), [](int32_t f) { return f == 3; });
			if (isTriangleMesh == false) {
				printf("skipped non-triangle mesh: %s\n", p->name.c_str());
				return;
			}

			glm::dmat4 xform;
			for (int i = 0; i < 16; ++i) {
				glm::value_ptr(xform)[i] = p->combinedXforms.value_ptr()[i];
			}
			glm::dmat3 xformInverseTransposed = glm::inverseTranspose(xform);

			// add index
			uint32_t base_index = _points.size();
			for (auto index : p->indices) {
				_indices.emplace_back(base_index + index);
			}
			// add vertex
			_points.reserve(_points.size() + p->P.size());
			for (auto srcP : p->P) {
				glm::vec3 p = xform * glm::dvec4(srcP.x, srcP.y, srcP.z, 1.0);
				_points.emplace_back(p);
			}

			RT_ASSERT(std::all_of(_indices.begin(), _indices.end(), [&](uint32_t index) { return index < _points.size(); }));

			add_materials(_material_storage.get(), p, xformInverseTransposed);
		}
		void addPoint(houdini_alembic::PointObject *p) {
			auto point_type = p->points.column_as_string("point_type");
			if (point_type == nullptr) {
				return;
			}
			for (int i = 0; i < point_type->rowCount(); ++i) {
				if (point_type->get(i) == "ImageEnvmap") {
					std::string filename;
					float clamp_max = std::numeric_limits<float>::max();
					if (auto r = p->points.column_as_string("file")) {
						filename = r->get(i);
					}
					if (auto r = p->points.column_as_float("clamp")) {
						clamp_max = r->get(i);
					}
					if (filename.empty() == false) {
#if USE_WHITE_FURNANCE_ENV
						auto image = std::shared_ptr<Image2D>(new Image2D());
						image->resize(2, 2);
						(*image)(0, 0) = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
						(*image)(1, 0) = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
						(*image)(0, 1) = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
						(*image)(1, 1) = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
						_envmapImage = image;
#else
						_envmapImage = load_image(filename);
#endif
						// _envmapImage->clamp_rgb(0.0f, 10000.0f);
						_envmapImage->clamp_rgb(0.0f, clamp_max);

						SCOPED_PROFILE("create envmap sampler");
						SET_PROFILE_DESC(filename.c_str());
						UniformDirectionWeight uniform_weight;
						_imageEnvmap = std::shared_ptr<ImageEnvmap>(new ImageEnvmap(_envmapImage, uniform_weight));
						_sixAxisImageEnvmap[0] = std::shared_ptr<ImageEnvmap>(new ImageEnvmap(_envmapImage, SixAxisDirectionWeight(CubeSection_XPlus)));
						_sixAxisImageEnvmap[1] = std::shared_ptr<ImageEnvmap>(new ImageEnvmap(_envmapImage, SixAxisDirectionWeight(CubeSection_XMinus)));
						_sixAxisImageEnvmap[2] = std::shared_ptr<ImageEnvmap>(new ImageEnvmap(_envmapImage, SixAxisDirectionWeight(CubeSection_YPlus)));
						_sixAxisImageEnvmap[3] = std::shared_ptr<ImageEnvmap>(new ImageEnvmap(_envmapImage, SixAxisDirectionWeight(CubeSection_YMinus)));
						_sixAxisImageEnvmap[4] = std::shared_ptr<ImageEnvmap>(new ImageEnvmap(_envmapImage, SixAxisDirectionWeight(CubeSection_ZPlus)));
						_sixAxisImageEnvmap[5] = std::shared_ptr<ImageEnvmap>(new ImageEnvmap(_envmapImage, SixAxisDirectionWeight(CubeSection_ZMinus)));
					}
				}
			}
		}
		std::shared_ptr<Image2D> load_image(std::string filename) const {
			std::filesystem::path filePath(filename);
			filePath.make_preferred();

			auto absFilePath = _alembicDirectory / filePath;

			auto image = std::shared_ptr<Image2D>(new Image2D());
			image->load(absFilePath.string().c_str());
			
			// Debug
			//image->resize(2, 2);
			//(*image)(0, 0) = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
			//(*image)(1, 0) = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);
			//(*image)(0, 1) = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
			//(*image)(1, 1) = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);

			return image;
		}

		void buildBVH() {
			RT_ASSERT(_indices.size() % 3 == 0);
			for (int i = 0; i < _indices.size(); i += 3) {
				TrianglePrimitive primitive;
				for (int j = 0; j < 3; ++j) {
					primitive.indices[j] = _indices[i + j];
				}
				_primitives.emplace_back(primitive);
			}

			std::vector<RTCBuildPrimitive> primitives;
			primitives.reserve(_primitives.size());

			for (int i = 0; i < _primitives.size(); ++i) {
				glm::vec3 min_value(FLT_MAX);
				glm::vec3 max_value(-FLT_MAX);
				for (int index : _primitives[i].indices) {
					auto P = _points[index].as_vec3();

					min_value = glm::min(min_value, P);
					max_value = glm::max(max_value, P);
				}
				RTCBuildPrimitive prim = {};
				prim.lower_x = min_value.x;
				prim.lower_y = min_value.y;
				prim.lower_z = min_value.z;
				prim.geomID = 0;
				prim.upper_x = max_value.x;
				prim.upper_y = max_value.y;
				prim.upper_z = max_value.z;
				prim.primID = i;
				primitives.emplace_back(prim);
			}
			_embreeBVH = std::shared_ptr<EmbreeBVH>(create_embreeBVH(primitives));
			_stacklessBVH = std::shared_ptr<StacklessBVH>(create_stackless_bvh(_embreeBVH.get()));
		}

		std::unique_ptr<SceneBuffer> createBuffer(cl_context context) const {
			std::unique_ptr<SceneBuffer> buffer(new SceneBuffer());
			buffer->top_aabb = _stacklessBVH->top_aabb;
			buffer->pointsCL = std::unique_ptr<OpenCLBuffer<OpenCLFloat3>>(new OpenCLBuffer<OpenCLFloat3>(context, _points.data(), _points.size(), OpenCLKernelBufferMode::ReadOnly));
			buffer->indicesCL = std::unique_ptr<OpenCLBuffer<uint32_t>>(new OpenCLBuffer<uint32_t>(context, _indices.data(), _indices.size(), OpenCLKernelBufferMode::ReadOnly));
			buffer->stacklessBVHNodesCL = std::unique_ptr<OpenCLBuffer<StacklessBVHNode>>(new OpenCLBuffer<StacklessBVHNode>(context, _stacklessBVH->nodes.data(), _stacklessBVH->nodes.size(), OpenCLKernelBufferMode::ReadOnly));
			buffer->primitive_idsCL = std::unique_ptr<OpenCLBuffer<uint32_t>>(new OpenCLBuffer<uint32_t>(context, _stacklessBVH->primitive_ids.data(), _stacklessBVH->primitive_ids.size(), OpenCLKernelBufferMode::ReadOnly));
			return buffer;
		}

		template <class T>
		std::unique_ptr<OpenCLBuffer<T>> createBufferSafe(cl_context context, const T *data, uint32_t size) const {
			if (size == 0) {
				T one;
				return std::unique_ptr<OpenCLBuffer<T>>(new OpenCLBuffer<T>(context, &one, 1, OpenCLKernelBufferMode::ReadOnly));
			}
			return std::unique_ptr<OpenCLBuffer<T>>(new OpenCLBuffer<T>(context, data, size, OpenCLKernelBufferMode::ReadOnly));
		}

		std::unique_ptr<MaterialBuffer> createMaterialBuffer(cl_context context) const {
			std::unique_ptr<MaterialBuffer> buffer(new MaterialBuffer());
			buffer->materials = createBufferSafe(context, _material_storage->materials.data(), _material_storage->materials.size());
			buffer->lambertians = createBufferSafe(context, _material_storage->lambertians.data(), _material_storage->lambertians.size());
			buffer->speculars = createBufferSafe(context, _material_storage->speculars.data(), _material_storage->speculars.size());
			buffer->dierectrics = createBufferSafe(context, _material_storage->dierectrics.data(), _material_storage->dierectrics.size());
			buffer->wards = createBufferSafe(context, _material_storage->wards.data(), _material_storage->wards.size());
			return buffer;
		}

		std::unique_ptr<EnvmapBuffer> createEnvmapBuffer(cl_context context) const {
			std::unique_ptr<EnvmapBuffer> buffer(new EnvmapBuffer());
			buffer->envmap = std::unique_ptr<OpenCLImage>(new OpenCLImage(context, _envmapImage->data(), _envmapImage->width(), _envmapImage->height()));
			
			int nPixels = _imageEnvmap->_pdf.size();
			std::vector<float> pdfs(nPixels);
			for (int i = 0; i < nPixels; ++i) {
				pdfs[i] = _imageEnvmap->_pdf[i];
			}
			buffer->pdfs = std::unique_ptr<OpenCLBuffer<float>>(new OpenCLBuffer<float>(context, pdfs.data(), pdfs.size(), OpenCLKernelBufferMode::ReadOnly));
			
			std::vector<EnvmapFragment> fragments(nPixels);
			for (int i = 0; i < nPixels; ++i) {
				fragments[i].beg_phi = _imageEnvmap->_fragments[i].beg_phi;
				fragments[i].end_phi = _imageEnvmap->_fragments[i].end_phi;
				fragments[i].beg_y   = _imageEnvmap->_fragments[i].beg_y;
				fragments[i].end_y   = _imageEnvmap->_fragments[i].end_y;
			}
			buffer->fragments = std::unique_ptr<OpenCLBuffer<EnvmapFragment>>(new OpenCLBuffer<EnvmapFragment>(context, fragments.data(), fragments.size(), OpenCLKernelBufferMode::ReadOnly));
			
			std::vector<AliasBucket> aliasBuckets(nPixels);
			for (int i = 0; i < nPixels; ++i) {
				aliasBuckets[i].height = _imageEnvmap->_aliasMethod.buckets[i].height;
				aliasBuckets[i].alias  = _imageEnvmap->_aliasMethod.buckets[i].alias;
			}
			buffer->aliasBuckets = std::unique_ptr<OpenCLBuffer<AliasBucket>>(new OpenCLBuffer<AliasBucket>(context, aliasBuckets.data(), aliasBuckets.size(), OpenCLKernelBufferMode::ReadOnly));

			// 6 Axis
			std::vector<float>       pdfN        (nPixels * 6);
			std::vector<AliasBucket> aliasBucketN(nPixels * 6);

			for (int axis = 0; axis < 6; ++axis) {
				auto env = _sixAxisImageEnvmap[axis];
				int base = nPixels * axis;
				for (int i = 0; i < nPixels; ++i) {
					pdfN[base + i] = env->_pdf[i];
				}
				for (int i = 0; i < nPixels; ++i) {
					aliasBucketN[base + i].height = env->_aliasMethod.buckets[i].height;
					aliasBucketN[base + i].alias  = env->_aliasMethod.buckets[i].alias;
				}
			}
			buffer->sixAxisPdfN = std::unique_ptr<OpenCLBuffer<float>>(new OpenCLBuffer<float>(context, pdfN.data(), pdfN.size(), OpenCLKernelBufferMode::ReadOnly));
			buffer->sixAxisAliasBucketN = std::unique_ptr<OpenCLBuffer<AliasBucket>>(new OpenCLBuffer<AliasBucket>(context, aliasBucketN.data(), aliasBucketN.size(), OpenCLKernelBufferMode::ReadOnly));

			return buffer;
		}

		std::filesystem::path _alembicDirectory;

		std::vector<uint32_t> _indices;
		std::vector<OpenCLFloat3> _points;
		std::shared_ptr<EmbreeBVH> _embreeBVH;
		std::shared_ptr<StacklessBVH> _stacklessBVH;

		// 現在は冗長
		std::vector<TrianglePrimitive> _primitives;

		// Material
		std::unique_ptr<MaterialStorage> _material_storage;

		std::shared_ptr<Image2D> _envmapImage;
		std::shared_ptr<ImageEnvmap> _imageEnvmap;
		std::shared_ptr<ImageEnvmap> _sixAxisImageEnvmap[6];
	};
}