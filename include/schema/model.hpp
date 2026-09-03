#pragma once
#include "view.hpp"

namespace mde::schema {
class Model {
private:
  std::unique_ptr<View> _view;
};
} // namespace mde::schema
