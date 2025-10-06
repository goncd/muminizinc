#ifndef MUTATION_HPP
#define MUTATION_HPP

#include <filesystem>  // std::filesystem::path
#include <optional>    // std::optional
#include <span>        // std::span
#include <string>      // std::string
#include <string_view> // std::string_view
#include <utility>     // std::move
#include <vector>      // std::vector

#include <minizinc/config.hh> // MZN_VERSION_MAJOR, MZN_VERSION_MINOR, MZN_VERSION_PATCH
#include <minizinc/model.hh>  // MiniZinc::Model

#define MZN_VERSION MZN_VERSION_MAJOR "." MZN_VERSION_MINOR "." MZN_VERSION_PATCH
#define MZN_VERSION_FULL "version " MZN_VERSION

#include <case_insensitive_string.hpp> // ascii_ci_string_view

class MutationModel
{
public:
    class OutdatedMutant : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    class IOError : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    // Constructs a MutationModel that works only in memory.
    explicit MutationModel(const std::filesystem::path& path, std::span<const ascii_ci_string_view> allowed_operators = {});

    // Constructs a MutationModel that works with a filesystem.
    explicit MutationModel(const std::filesystem::path& path, std::string_view output_directory, std::span<const ascii_ci_string_view> allowed_operators = {});

    bool find_mutants(std::string&& include_path);

    void clear_output_folder();

    void clear_memory() noexcept;

    struct Entry
    {
        enum class Status : std::uint8_t
        {
            Alive,
            Dead,
            Invalid
        };

        Entry(std::string name, std::string contents) noexcept :
            name { std::move(name) }, contents { std::move(contents) } { }

        Entry() noexcept = default;

        std::string name;
        std::string contents;

        // The result of the mutant tested against every data file.
        std::vector<Status> results;
    };

    [[nodiscard]] std::span<const Entry> run_mutants(const std::filesystem::path& compiler_path, std::span<const std::string_view> compiler_arguments, std::span<const std::string> data_files, std::chrono::seconds timeout, std::uint64_t n_jobs, std::span<const ascii_ci_string_view> mutants, bool check_compiler_version, bool check_model_last_modified_time);

    [[nodiscard]] static std::span<const std::pair<std::string_view, std::string_view>> get_available_operators();

    static constexpr std::string_view get_version() noexcept { return MZN_VERSION; }
    static constexpr std::string_view get_version_full() noexcept { return MZN_VERSION_FULL; }

private:
    class Mutator;
    void save_current_model(std::string_view mutant_name, std::uint64_t mutant_id, std::uint64_t occurrence_id);

    std::filesystem::path m_model_path, m_mutation_folder_path;
    std::string m_filename_stem;

    MiniZinc::Model* m_model = nullptr;

    // It's guaranteed that the first element is the original model.
    std::vector<Entry> m_memory;

    std::span<const ascii_ci_string_view> m_allowed_operators;

    // Returns the STEM of the given directory entry if it's a valid mutant or the original file
    [[nodiscard]] std::optional<std::string> get_stem_if_valid(const std::filesystem::directory_entry& entry) const noexcept;

    std::vector<std::pair<std::string, std::string>> m_detected_enums;
    void fix_enums(std::string& model);
};

#endif