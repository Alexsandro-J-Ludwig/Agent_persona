"""Regressões da busca local; não exigem Ollama, COM ou documentos reais."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]

def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / f'{name}.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

arquivos = load('arquivos')
documentos = load('documentos')

class BuscaTests(unittest.TestCase):
    def test_listar_diretorio_esta_disponivel_para_o_agente(self):
        # O nome anunciado ao modelo deve coincidir com a chave do registro.
        from tools import AVAILABLE_TOOLS, tools as schemas

        names = {tool['function']['name'] for tool in schemas}
        self.assertIn('listar_diretorio', names)
        self.assertTrue(callable(AVAILABLE_TOOLS['listar_diretorio']))
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'sub').mkdir()
            file = root / 'relatorio.txt'
            file.write_text('texto', encoding='utf-8')
            result = AVAILABLE_TOOLS['listar_diretorio'](folder)
            self.assertEqual(
                {(item['nome'], item['tipo']) for item in result},
                {('sub', 'diretorio'), ('relatorio.txt', 'arquivo')},
            )
            self.assertEqual(
                AVAILABLE_TOOLS['listar_diretorio'](str(root / 'ausente'))[0]['erro'],
                'Diretório não encontrado',
            )
            self.assertEqual(
                AVAILABLE_TOOLS['listar_diretorio'](str(file))[0]['erro'],
                'O caminho informado não é um diretório',
            )

    def test_busca_e_leitura(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'sub').mkdir()
            file = root / 'sub' / 'Relatório_de_Vendas.TXT'
            file.write_text('Documento encontrado', encoding='utf-8')
            (root / 'outro.txt').write_text('outro', encoding='utf-8')
            for termo in ['relatorio vendas', 'vendas relatorio', 'relatorio de vendas.txt', 'relatrio vendas']:
                with self.subTest(termo=termo):
                    result = arquivos.pesquisar_arquivo(termo, folder)
                    self.assertTrue(result['completa'])
                    self.assertEqual([r['caminho'] for r in result['resultados']], [str(file)])
            leitura = documentos.ler_documento(str(file))
            self.assertEqual(leitura['conteudo'], 'Documento encontrado')
            self.assertFalse(leitura['tem_mais'])
            self.assertEqual(arquivos.pesquisar_arquivo('inexistente', folder)['resultados'], [])
            self.assertFalse(arquivos.pesquisar_arquivo('', folder)['completa'])
            self.assertFalse(arquivos.pesquisar_arquivo('arquivo', str(root / 'ausente'))['completa'])

    def test_default_usa_pasta_pessoal(self):
        with tempfile.TemporaryDirectory() as folder:
            Path(folder, 'contrato.txt').touch()
            with patch.object(arquivos.Path, 'home', return_value=Path(folder)):
                self.assertEqual(len(arquivos.pesquisar_arquivo('contrato')['resultados']), 1)

    def test_limite_informa_busca_parcial(self):
        with tempfile.TemporaryDirectory() as folder:
            Path(folder, 'contrato.txt').touch()
            with patch.object(arquivos.time, 'monotonic', side_effect=[0, 20, 20]):
                result = arquivos.pesquisar_arquivo('contrato', folder)
            self.assertFalse(result['completa'])
            self.assertIn('Limite', result['avisos'][0])

    def test_documento_grande_em_blocos_e_busca_textual(self):
        with tempfile.TemporaryDirectory() as folder:
            file = Path(folder, 'grande.txt')
            file.write_text('a' * 4094 + 'MARCADOR' + 'b' * 12000, encoding='utf-8')
            partes = []
            inicio = 0
            while True:
                resposta = documentos.ler_documento(str(file), inicio=inicio)
                self.assertLessEqual(len(resposta['conteudo']), documentos.TAMANHO_PADRAO)
                partes.append(resposta['conteudo'])
                if not resposta['tem_mais']:
                    break
                inicio = resposta['proximo_inicio']
            self.assertEqual(''.join(partes), file.read_text(encoding='utf-8'))
            resultado = documentos.buscar_documento(str(file), 'MARCADOR')
            self.assertTrue(resultado['busca_completa'])
            self.assertEqual([r['inicio'] for r in resultado['trechos']], [4094])

    def test_planilha_lida_em_blocos(self):
        from openpyxl import Workbook

        with tempfile.TemporaryDirectory() as folder:
            file = Path(folder, 'planilha.xlsx')
            livro = Workbook()
            aba = livro.active
            aba.title = 'Dados'
            for numero in range(100):
                aba.append([numero, f'Colaborador {numero}'])
            livro.save(file)
            primeira = documentos.ler_documento(str(file), tamanho=150)
            self.assertTrue(primeira['tem_mais'])
            self.assertIn('[Aba: Dados]', primeira['conteudo'])
            proxima = documentos.ler_documento(str(file), inicio=primeira['proximo_inicio'], tamanho=150)
            self.assertNotEqual(primeira['conteudo'], proxima['conteudo'])
            resultado = documentos.buscar_documento(str(file), 'Colaborador 99')
            self.assertEqual(len(resultado['trechos']), 1)
            self.assertIn('Colaborador 99', resultado['trechos'][0]['trecho'])

    def test_docx_lido_em_blocos_inclui_tabela(self):
        from zipfile import ZipFile

        with tempfile.TemporaryDirectory() as folder:
            file = Path(folder, 'documento.docx')
            # XML mínimo reproduz parágrafos e células de um DOCX real.
            with ZipFile(file, 'w') as pacote:
                pacote.writestr('word/document.xml', '''
                    <w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
                      <w:body><w:p><w:r><w:t>Introdução</w:t></w:r></w:p>
                        <w:tbl><w:tr><w:tc><w:p><w:r><w:t>Chave</w:t></w:r></w:p></w:tc>
                          <w:tc><w:p><w:r><w:t>Valor procurado</w:t></w:r></w:p></w:tc>
                        </w:tr></w:tbl>
                      </w:body>
                    </w:document>
                ''')
            primeira = documentos.ler_documento(str(file), tamanho=12)
            self.assertTrue(primeira['tem_mais'])
            resultado = documentos.buscar_documento(str(file), 'Valor procurado')
            self.assertEqual(len(resultado['trechos']), 1)

    def test_ferramentas_de_documento_registradas(self):
        from tools import AVAILABLE_TOOLS, tools as schemas

        anunciadas = {item['function']['name'] for item in schemas}
        for nome in ('ler_documento', 'buscar_documento'):
            self.assertIn(nome, anunciadas)
            self.assertTrue(callable(AVAILABLE_TOOLS[nome]))

if __name__ == '__main__':
    unittest.main()
