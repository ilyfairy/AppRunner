﻿using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using AppRunner.Models;
using Windows.Storage;

namespace AppRunner.Services
{
    public class ConfigurationService
    {
        private AppConfiguration? _loadedConfiguration;
        private StorageFolder _localStorageFolder;

        public string ConfigurationFileName => "AppConfiguration.json";
        public AppConfiguration Configuration => _loadedConfiguration ?? throw new InvalidOperationException("Configuration not loaded");

        public ConfigurationService()
        {
            _localStorageFolder = ApplicationData.Current.LocalFolder;
        }

        public async Task LoadConfiguration()
        {
            var fileItem = await _localStorageFolder.TryGetItemAsync(ConfigurationFileName);

            if (fileItem is not StorageFile file)
            {
                _loadedConfiguration = new AppConfiguration();
                return;
            }

            try
            {
                using var fileStream = await file.OpenReadAsync();
                using var fileStreamReader = new StreamReader(fileStream.AsStreamForRead());
                var configurationText = await fileStreamReader.ReadToEndAsync();
                var configuration = JsonSerializer.Deserialize<AppConfiguration>(configurationText);

                _loadedConfiguration = configuration ?? new();
            }
            catch
            {
                _loadedConfiguration = new();
            }
        }

        public async Task SaveConfiguration()
        {
            var configurationText = JsonSerializer.Serialize(Configuration);
            var file = await _localStorageFolder.CreateFileAsync(ConfigurationFileName, CreationCollisionOption.ReplaceExisting);
            using var fileStream = await file.OpenStreamForWriteAsync();
            using var fileStreamWriter = new StreamWriter(fileStream);

            await fileStreamWriter.WriteAsync(configurationText);
        }
    }
}
