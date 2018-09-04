#include <cassert>
#include <cstdio>
#include <string>
#include <algorithm>
#include "ExportObj.h"
#include "FindConnections.h"
#include "Store.h"

namespace {

  bool open_w(FILE** f, const char* path)
  {
    auto err = fopen_s(f, path, "w");
    if (err == 0) return true;

    char buf[1024];
    if (strerror_s(buf, sizeof(buf), err) != 0) {
      buf[0] = '\0';
    }
    fprintf(stderr, "Failed to open %s for writing: %s", path, buf);
    return false;
  }

  void wireBoundingBox(FILE* out, unsigned& off_v, const float* bbox)
  {
    for (unsigned i = 0; i < 8; i++) {
      float px = (i & 1) ? bbox[0] : bbox[3];
      float py = (i & 2) ? bbox[1] : bbox[4];
      float pz = (i & 4) ? bbox[2] : bbox[5];
      fprintf(out, "v %f %f %f\n", px, py, pz);
    }
    fprintf(out, "l %d %d %d %d %d\n",
            off_v + 0, off_v + 1, off_v + 3, off_v + 2, off_v + 0);
    fprintf(out, "l %d %d %d %d %d\n",
            off_v + 4, off_v + 5, off_v + 7, off_v + 6, off_v + 4);
    fprintf(out, "l %d %d\n", off_v + 0, off_v + 4);
    fprintf(out, "l %d %d\n", off_v + 1, off_v + 5);
    fprintf(out, "l %d %d\n", off_v + 2, off_v + 6);
    fprintf(out, "l %d %d\n", off_v + 3, off_v + 7);
    off_v += 8;
  }

}


ExportObj::~ExportObj()
{
  if (out) {
    fclose(out);
  }
  if (mtl) {
    fclose(mtl);
  }
}

bool ExportObj::open(const char* path_obj, const char* path_mtl)
{
  if (!open_w(&out, path_obj)) return false;
  if (!open_w(&mtl, path_mtl)) return false;

  std::string mtllib(path_mtl);
  auto l = mtllib.find_last_of("/\\");
  if (l != std::string::npos) {
    mtllib = mtllib.substr(l + 1);
  }

  fprintf(out, "mtllib %s\n", mtllib.c_str());

  if (groupBoundingBoxes) {
    fprintf(mtl, "newmtl group_bbox\n");
    fprintf(mtl, "Ka 1 0 0\n");
    fprintf(mtl, "Kd 1 0 0\n");
    fprintf(mtl, "Ks 0 0 0\n");
  }

  return true;
}


void ExportObj::init(class Store& store)
{
  assert(out);
  assert(mtl);

  conn = store.conn;
}

void ExportObj::beginFile(Group* group)
{
  fprintf(out, "# %s\n", group->file.info);
  fprintf(out, "# %s\n", group->file.note);
  fprintf(out, "# %s\n", group->file.date);
  fprintf(out, "# %s\n", group->file.user);
}

void ExportObj::endFile() {

  //fprintf(out, "usemtl red_line\n");

  //auto l = 0.05f;
  //if (anchors && conn) {
  //  for (unsigned i = 0; i < conn->anchor_n; i++) {
  //    auto * p = conn->p + 3 * i;
  //    auto * n = conn->anchors[i].n;

  //    fprintf(out, "v %f %f %f\n", p[0], p[1], p[2]);
  //    fprintf(out, "v %f %f %f\n", p[0] + l * n[0], p[1] + l * n[1], p[2] + l * n[2]);
  //    fprintf(out, "l %d %d\n", (int)off_v, (int)off_v + 1);
  //    off_v += 2;
  //  }
  //}

  fprintf(out, "# End of file\n");
}

void ExportObj::beginModel(Group* group)
{
  fprintf(out, "# Model project=%s, name=%s\n", group->model.project, group->model.name);
}

void ExportObj::endModel() { }

void ExportObj::beginGroup(Group* group)
{
  for (unsigned i = 0; i < 3; i++) curr_translation[i] = group->group.translation[i];

  fprintf(out, "o %s\n", group->group.name);
  if (groupBoundingBoxes && group->group.bbox) {
    fprintf(out, "usemtl group_bbox\n");
    wireBoundingBox(out, off_v, group->group.bbox);
  }

}

void ExportObj::EndGroup() { }

namespace {

  void getMidpoint(float* p, Geometry* geo)
  {
    const auto & M = geo->M_3x4;

    float px = 0.f;
    float py = 0.f;
    float pz = 0.f;

    switch (geo->kind) {
    case Geometry::Kind::CircularTorus: {
      auto & ct = geo->circularTorus;
      auto c = std::cos(0.5f * ct.angle);
      auto s = std::sin(0.5f * ct.angle);
      px = ct.offset * c;
      py = ct.offset * s;
      pz = 0.f;
      break;
    }

    default:
      break;
    }

    p[0] = M[0] * px + M[3] * py + M[6] * pz + M[9];
    p[1] = M[1] * px + M[4] * py + M[7] * pz + M[10];
    p[2] = M[2] * px + M[5] * py + M[8] * pz + M[11];

  }

}

void ExportObj::geometry(struct Geometry* geometry)
{
  const auto & M = geometry->M_3x4;

  //if (geometry->composite && geometry->composite->size < 0.5f) return;

  if (!definedColors.get(uint64_t(geometry->colorName))) {
    definedColors.insert(uint64_t(geometry->colorName), 1);

    auto r = (1.f / 255.f)*((geometry->color >> 16) & 0xFF);
    auto g = (1.f / 255.f)*((geometry->color >> 8) & 0xFF);
    auto b = (1.f / 255.f)*((geometry->color) & 0xFF);

    fprintf(mtl, "newmtl %s\n", geometry->colorName);
    fprintf(mtl, "Ka %f %f %f\n", (2.f / 3.f)*r, (2.f / 3.f)*g, (2.f / 3.f)*b);
    fprintf(mtl, "Kd %f %f %f\n", r,g, b);
    fprintf(mtl, "Ks 0.5 0.5 0.5\n");
  }

  fprintf(out, "usemtl %s\n", geometry->colorName);

  
  if (geometry->kind == Geometry::Kind::Line) {
    auto x0 = geometry->line.a;
    auto x1 = geometry->line.b;

    auto p0_x = M[0] * x0 + M[3] * 0.f + M[6] * 0.f + M[9];
    auto p0_y = M[1] * x0 + M[4] * 0.f + M[7] * 0.f + M[10];
    auto p0_z = M[2] * x0 + M[5] * 0.f + M[8] * 0.f + M[11];

    auto p1_x = M[0] * x1 + M[3] * 0.f + M[6] * 0.f + M[9];
    auto p1_y = M[1] * x1 + M[4] * 0.f + M[7] * 0.f + M[10];
    auto p1_z = M[2] * x1 + M[5] * 0.f + M[8] * 0.f + M[11];

    fprintf(out, "v %f %f %f\n", p0_x, p0_y, p0_z);
    fprintf(out, "v %f %f %f\n", p1_x, p1_y, p1_z);
    fprintf(out, "l -1 -2\n");

    off_v += 2;
  }
  else if (geometry->triangulation != nullptr) {
    auto * tri = geometry->triangulation;

    //fprintf(out, "g\n");
    if(geometry->triangulation->error != 0.f) {
      fprintf(out, "# error=%f\n", geometry->triangulation->error);
    }
    for (size_t i = 0; i < 3 * tri->vertices_n; i += 3) {
      auto px = tri->vertices[i + 0];
      auto py = tri->vertices[i + 1];
      auto pz = tri->vertices[i + 2];
      auto nx = tri->normals[i + 0];
      auto ny = tri->normals[i + 1];
      auto nz = tri->normals[i + 2];

      float Px, Py, Pz, Nx, Ny, Nz;
      Px = M[0] * px + M[3] * py + M[6] * pz + M[9];
      Py = M[1] * px + M[4] * py + M[7] * pz + M[10];
      Pz = M[2] * px + M[5] * py + M[8] * pz + M[11];
      Nx = M[0] * nx + M[3] * ny + M[6] * nz;
      Ny = M[1] * nx + M[4] * ny + M[7] * nz;
      Nz = M[2] * nx + M[5] * ny + M[8] * nz;

      float s = 1.f / std::sqrt(Nx*Nx + Ny * Ny + Nz * Nz);


      if (true) {
        fprintf(out, "v %f %f %f\n", Px, Py, Pz);
        fprintf(out, "vn %f %f %f\n", s*Nx, s*Ny, s*Nz);
      }
      else {
        fprintf(out, "v %f %f %f\n", px, py, pz);
        fprintf(out, "vn %f %f %f\n", nx, ny, nz);
      }
    }
    for (size_t i = 0; i < 3*tri->triangles_n; i += 3) {
      fprintf(out, "f %d//%d %d//%d %d//%d\n",
              tri->indices[i + 0] + off_v, tri->indices[i + 0] + off_n,
              tri->indices[i + 1] + off_v, tri->indices[i + 1] + off_n,
              tri->indices[i + 2] + off_v, tri->indices[i + 2] + off_n);
    }
    off_v += tri->vertices_n;
    off_n += tri->vertices_n;
  }

  //if (primitiveBoundingBoxes) {
  //  fprintf(out, "usemtl magenta\n");

  //  for (unsigned i = 0; i < 8; i++) {
  //    float px = (i & 1) ? geometry->bbox[0] : geometry->bbox[3];
  //    float py = (i & 2) ? geometry->bbox[1] : geometry->bbox[4];
  //    float pz = (i & 4) ? geometry->bbox[2] : geometry->bbox[5];

  //    float Px = M[0] * px + M[3] * py + M[6] * pz + M[9];
  //    float Py = M[1] * px + M[4] * py + M[7] * pz + M[10];
  //    float Pz = M[2] * px + M[5] * py + M[8] * pz + M[11];

  //    fprintf(out, "v %f %f %f\n", Px, Py, Pz);
  //  }
  //  fprintf(out, "l %d %d %d %d %d\n",
  //          off_v + 0, off_v + 1, off_v + 3, off_v + 2, off_v + 0);
  //  fprintf(out, "l %d %d %d %d %d\n",
  //          off_v + 4, off_v + 5, off_v + 7, off_v + 6, off_v + 4);
  //  fprintf(out, "l %d %d\n", off_v + 0, off_v + 4);
  //  fprintf(out, "l %d %d\n", off_v + 1, off_v + 5);
  //  fprintf(out, "l %d %d\n", off_v + 2, off_v + 6);
  //  fprintf(out, "l %d %d\n", off_v + 3, off_v + 7);
  //  off_v += 8;
  //}


  //for (unsigned k = 0; k < 6; k++) {
  //  auto other = geometry->conn_geo[k];
  //  if (geometry < other) {
  //    fprintf(out, "usemtl blue_line\n");
  //    float p[3];
  //    getMidpoint(p, geometry);
  //    fprintf(out, "v %f %f %f\n", p[0], p[1], p[2]);
  //    getMidpoint(p, other);
  //    fprintf(out, "v %f %f %f\n", p[0], p[1], p[2]);
  //    fprintf(out, "l %d %d\n", off_v, off_v + 1);

  //    off_v += 2;
  //  }
  //}

}


void ExportObj::composite(struct Composite* comp)
{
  //if (compositeBoundingBoxes == false) return;

  //if (comp->size < 0.5f) {
  //  fprintf(out, "usmtl magenta\n");
  //}
  //else {
  //  fprintf(out, "usemtl green\n");
  //}

  //for (unsigned i = 0; i < 8; i++) {
  //  float px = (i & 1) ? comp->bbox[0] : comp->bbox[3];
  //  float py = (i & 2) ? comp->bbox[1] : comp->bbox[4];
  //  float pz = (i & 4) ? comp->bbox[2] : comp->bbox[5];
  //  fprintf(out, "v %f %f %f\n", px, py, pz);
  //}
  //fprintf(out, "l %d %d %d %d %d\n",
  //        off_v + 0, off_v + 1, off_v + 3, off_v + 2, off_v + 0);
  //fprintf(out, "l %d %d %d %d %d\n",
  //        off_v + 4, off_v + 5, off_v + 7, off_v + 6, off_v + 4);
  //fprintf(out, "l %d %d\n", off_v + 0, off_v + 4);
  //fprintf(out, "l %d %d\n", off_v + 1, off_v + 5);
  //fprintf(out, "l %d %d\n", off_v + 2, off_v + 6);
  //fprintf(out, "l %d %d\n", off_v + 3, off_v + 7);
  //off_v += 8;
}
