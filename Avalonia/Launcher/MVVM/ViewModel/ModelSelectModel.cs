using Breath_of_the_Wild_Multiplayer.MVVM.Model;
using Breath_of_the_Wild_Multiplayer.MVVM.Model.DTO;
using Breath_of_the_Wild_Multiplayer.Source_files;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;

namespace Breath_of_the_Wild_Multiplayer.MVVM.ViewModel
{
    public class ModelSelectModel : ObservableObject
    {
        private ObservableCollection<CharacterModel> _characters;

        public ObservableCollection<CharacterModel> Characters
        {
            get { return _characters; }
            set
            {
                _characters = value;
                OnPropertyChanged();
            }
        }

        private bool _isMaxOffset;

        public bool isMaxOffset
        {
            get { return _isMaxOffset; }
            set
            {
                _isMaxOffset = value;
                OnPropertyChanged();
            }
        }

        private int _scrollState;

        public int scrollState
        {
            get { return _scrollState; }
            set
            {
                _scrollState = value;
                OnPropertyChanged();
            }
        }

        private CharacterModel _selectedModel;

        public CharacterModel SelectedModel
        {
            get { return _selectedModel; }
            set { 
                _selectedModel = value;
                OnPropertyChanged();
            }
        }

        private CharacterModel _lastSelectedModel;

        public CharacterModel LastSelectedModel
        {
            get { return _lastSelectedModel; }
            set
            {
                _lastSelectedModel = value;
                OnPropertyChanged();
            }
        }

        // Full-body picture of the selected model, shown next to the list.
        public string BodyPic => this.SelectedModel?.BodyPic;

        public RelayCommand modelButtonClick { get; set; }

        public ModelSelectModel()
        {
            _characters = new ObservableCollection<CharacterModel>();

            LoadModels();

            SharedData.ModelSelect = this;

            modelButtonClick = new RelayCommand(o =>
            {
                this.ChangeSelectedModel(_characters[(int)o]);
                Properties.Settings.Default.playerModel = JsonConvert.SerializeObject(this.SelectedModel);
                Properties.Settings.Default.Save();
            });
        }

        private void LoadModels()
        {
            CharacterModel.modelsLoaded = 0;

            _characters.Add(new CharacterModel("Link", "Jugador1ModelNameLongForASpecificReason", true));
            _characters.Add(new CharacterModel("BumiiMaker", "Bumii", false, "Create your own character using bumii maker and import them!", new RelayCommand(o => this.LoadBumii()), alternativeModel: "BumiiMaker"));
            _characters.Add(new CharacterModel("Environmental", "", false, "Select any model that appears in the game.", new RelayCommand(o => this.LoadEnvironmental()), alternativeModel: "Environmental"));

            string fileContents = string.Empty;

            using(var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("Breath_of_the_Wild_Multiplayer.Resources.NpcData.json"))
            {
                TextReader tr = new StreamReader(stream);
                fileContents = tr.ReadToEnd();
            }

            List<NpcModel> loadedNpcs = JsonConvert.DeserializeObject<List<NpcModel>>(fileContents);

            foreach(NpcModel npc in loadedNpcs)
                _characters.Add(new CharacterModel(npc.Name, npc.Folder, false));

            if (Properties.Settings.Default.playerModel == "")
            {
                this.SelectedModel = _characters[0];
                this.LastSelectedModel = _characters[0];

                this.LastSelectedModel.Selected = false;
                this.SelectedModel.Selected = true;
            }
            else
            {
                CharacterModel playerModel = JsonConvert.DeserializeObject<CharacterModel>(Properties.Settings.Default.playerModel);

                this.SelectedModel = _characters[playerModel.ModelIndex];
                this.LastSelectedModel = this.SelectedModel;

                this.LastSelectedModel.Selected = false;
                this.SelectedModel.Selected = true;

                this.SelectedModel.Model = playerModel.Model;
                this.SelectedModel.Name = playerModel.Name;
                this.SelectedModel.BumiiData = playerModel.BumiiData;
                this.SelectedModel.Description = playerModel.Description;
            }

            this.UpdateBodyImage();
        }

        public void ChangeSelectedModel(CharacterModel newSelection)
        {
            this.LastSelectedModel = SelectedModel;
            SelectedModel = newSelection;

            this.LastSelectedModel.Selected = false;
            newSelection.Selected = true;

            if (SelectedModel.HasCustomAction)
                SelectedModel.CustomAction.Execute(null);

            this.UpdateBodyImage();
        }

        public async void LoadBumii()
        {
            Tuple<string, BumiiDTO> result;
            try
            {
                result = await BumiiLoader.readBumii();
            }
            catch (Exception ex)
            {
                SharedData.LoadErrorMessage(ex.Message);
                this.ChangeSelectedModel(this.LastSelectedModel);
                return;
            }

            if(result == null)
            {
                this.ChangeSelectedModel(this.LastSelectedModel);
                return;
            }

            this.SelectedModel.BumiiData = result.Item2;
            this.SelectedModel.Type = CharacterModel.ModelType.Mii;
            this.SelectedModel.Name = Path.GetFileNameWithoutExtension(result.Item1);
            this.SelectedModel.UmiiPath = result.Item1;
        }

        public void LoadEnvironmental()
        {
            SharedData.MainView.updateTopView(new EnvironmentalSelectorModel());
        }

        public void UpdateBodyImage()
        {
            OnPropertyChanged(nameof(BodyPic));
        }

        public void SetEnvironmental(CharacterModel environmentalModel)
        {
            this.Characters[2].Model = environmentalModel.Model;
            this.Characters[2].Name = environmentalModel.Name;
            this.Characters[2].Description = $"[{environmentalModel.Description}]\nSelect any model that appears in the game.";
            Properties.Settings.Default.playerModel = JsonConvert.SerializeObject(this.SelectedModel);
            Properties.Settings.Default.Save();
        }
    }
}
