#ifndef MUTATION_HPP
#define MUTATION_HPP

#include <string_view>

#include <minizinc/model.hh> // MiniZinc::Model

class MutationModel
{
public:
    explicit MutationModel(std::string_view path);

    void find_mutants();

    [[nodiscard]] std::string_view mutation_original_filename_stem() const noexcept { return m_filename_stem; }
    [[nodiscard]] std::string_view mutation_folder_path() const noexcept { return m_mutation_folder_path; }

    void save_current_model(const char* path);

private:
    std::string m_filename_stem, m_mutation_folder_path;

    MiniZinc::Model* m_model;
};

#endif