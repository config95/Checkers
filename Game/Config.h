#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

//// Класс для загрузки и доступа к настройкам из settings.json.
/// Хранит JSON-объект и предоставляет простой доступ к параметрам по секции/ключу.
class Config
{
  public:
    Config()
    {
        reload();
    }

    /** 
     * Открывает файл settings.json относительно project_path,
     * парсит JSON (nlohmann::json) и сохраняет результат в поле config.
     * Вызывать при старте игры и при изменении файла настроек.
     */
    void reload()
    {
        std::ifstream fin(project_path + "settings.json");
        if (!fin) {
            throw std::runtime_error("Не удалось открыть settings.json");
        }

        // Читаем всё содержимое файла в строку
        std::string content((std::istreambuf_iterator<char>(fin)),
                            std::istreambuf_iterator<char>());
        fin.close();

        // Разбираем, разрешив комментарии
        config = json::parse(content, nullptr, true,
                             true);
    }


  /*
   * Возвращает значение настройки по секции и ключу.
   */
    auto operator()(const string &setting_dir, const string &setting_name) const
    {
        return config[setting_dir][setting_name];
    }

  private:
    json config;
};
