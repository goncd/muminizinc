#ifndef MUTATION_HPP
#define MUTATION_HPP

#include <filesystem>
#include <string_view>

#include <minizinc/model.hh> // MiniZinc::Model

class MutationModel
{
public:
    explicit MutationModel(std::string_view path);

    void find_mutants();

    [[nodiscard]] const MiniZinc::Model* model() const noexcept { return m_model; }

private:
    std::filesystem::path m_path;

    MiniZinc::Model* m_model;
};

#endif